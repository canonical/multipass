/*
 * QEMU Bridge Helper
 *
 * Copyright IBM, Corp. 2011
 * Copyright (C) Canonical, Ltd.
 *
 * Authors:
 * Anthony Liguori   <aliguori@us.ibm.com>
 * Richa Marwaha     <rmarwah@linux.vnet.ibm.com>
 * Corey Bryant      <coreyb@linux.vnet.ibm.com>
 * Luis Peñaranda    <luis.penaranda@canonical.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

/* This version of the bridge helper was adapted for use with Multipass.
   The changes are:
   - the authorization via ACL was removed;
   - dependencies on other QEMU functions were replaced by common includes;
   - functionality was wrapped inside a function.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>

#include <linux/sockios.h>

#ifndef SIOCBRADDIF
#include <linux/if_bridge.h>
#endif

#ifdef CONFIG_LIBCAP_NG
#include <cap-ng.h>
#endif

static void usage(void)
{
    fprintf(stderr, "Usage: bridge_helper [--use-vnet] --br=bridge --fd=unixfd\n");
}

static bool has_vnet_hdr(int fd)
{
    unsigned int features = 0;

    if (ioctl(fd, TUNGETFEATURES, &features) == -1)
    {
        return false;
    }

    if (!(features & IFF_VNET_HDR))
    {
        return false;
    }

    return true;
}

static void prep_ifreq(struct ifreq* ifr, const char* ifname)
{
    memset(ifr, 0, sizeof(*ifr));
    snprintf(ifr->ifr_name, IFNAMSIZ, "%s", ifname);
}

static int send_fd(int c, int fd)
{
    char msgbuf[CMSG_SPACE(sizeof(fd))];
    struct msghdr msg = {
        .msg_control = msgbuf,
        .msg_controllen = sizeof(msgbuf),
    };
    struct cmsghdr* cmsg;
    struct iovec iov;
    char req[1] = {0x00};

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    msg.msg_controllen = cmsg->cmsg_len;

    iov.iov_base = req;
    iov.iov_len = sizeof(req);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

    return sendmsg(c, &msg, 0);
}

#ifdef CONFIG_LIBCAP_NG
static int drop_privileges(void)
{
    /* clear all capabilities */
    capng_clear(CAPNG_SELECT_BOTH);

    if (capng_update(CAPNG_ADD, CAPNG_EFFECTIVE | CAPNG_PERMITTED, CAP_NET_ADMIN) < 0)
    {
        return -1;
    }

    /* change to calling user's real uid and gid, retaining supplemental
     * groups and CAP_NET_ADMIN */
    if (capng_change_id(getuid(), getgid(), CAPNG_CLEAR_BOUNDING))
    {
        return -1;
    }

    return 0;
}
#endif

int bridge_helper(const char* bridge, const int unixfd, const int use_vnet)
{
    struct ifreq ifr;
#ifndef SIOCBRADDIF
    unsigned long ifargs[4];
#endif
    int ifindex;
    int fd = -1, ctlfd = -1;
    int mtu;
    char iface[IFNAMSIZ];
    int ret = EXIT_SUCCESS;

#ifdef CONFIG_LIBCAP_NG
    /* if we're run from an suid binary, immediately drop privileges preserving
     * cap_net_admin */
    if (geteuid() == 0 && getuid() != geteuid())
    {
        if (drop_privileges() == -1)
        {
            fprintf(stderr, "failed to drop privileges\n");
            return 1;
        }
    }
#endif

    if (bridge == NULL || unixfd == -1)
    {
        usage();
        return EXIT_FAILURE;
    }
    if (strlen(bridge) >= IFNAMSIZ)
    {
        fprintf(stderr, "name `%s' too long: %zu\n", bridge, strlen(bridge));
        return EXIT_FAILURE;
    }

    /* open a socket to use to control the network interfaces */
    ctlfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctlfd == -1)
    {
        fprintf(stderr, "failed to open control socket: %s\n", strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* open the tap device */
    fd = open("/dev/net/tun", O_RDWR);
    if (fd == -1)
    {
        fprintf(stderr, "failed to open /dev/net/tun: %s\n", strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* request a tap device, disable PI, and add vnet header support if
     * requested and it's available. */
    prep_ifreq(&ifr, "tap%d");
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (use_vnet && has_vnet_hdr(fd))
    {
        ifr.ifr_flags |= IFF_VNET_HDR;
    }

    if (ioctl(fd, TUNSETIFF, &ifr) == -1)
    {
        fprintf(stderr, "failed to create tun device: %s\n", strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* save tap device name */
    snprintf(iface, sizeof(iface), "%s", ifr.ifr_name);

    /* get the mtu of the bridge */
    prep_ifreq(&ifr, bridge);
    if (ioctl(ctlfd, SIOCGIFMTU, &ifr) == -1)
    {
        fprintf(stderr, "failed to get mtu of bridge `%s': %s\n", bridge, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* save mtu */
    mtu = ifr.ifr_mtu;

    /* set the mtu of the interface based on the bridge */
    prep_ifreq(&ifr, iface);
    ifr.ifr_mtu = mtu;
    if (ioctl(ctlfd, SIOCSIFMTU, &ifr) == -1)
    {
        fprintf(stderr, "failed to set mtu of device `%s' to %d: %s\n", iface, mtu, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* Linux uses the lowest enslaved MAC address as the MAC address of
     * the bridge.  Set MAC address to a high value so that it doesn't
     * affect the MAC address of the bridge.
     */
    if (ioctl(ctlfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        fprintf(stderr, "failed to get MAC address of device `%s': %s\n", iface, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    ifr.ifr_hwaddr.sa_data[0] = (char)0xFE;
    if (ioctl(ctlfd, SIOCSIFHWADDR, &ifr) < 0)
    {
        fprintf(stderr, "failed to set MAC address of device `%s': %s\n", iface, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* add the interface to the bridge */
    prep_ifreq(&ifr, bridge);
    ifindex = if_nametoindex(iface);
#ifndef SIOCBRADDIF
    ifargs[0] = BRCTL_ADD_IF;
    ifargs[1] = ifindex;
    ifargs[2] = 0;
    ifargs[3] = 0;
    ifr.ifr_data = (void*)ifargs;
    ret = ioctl(ctlfd, SIOCDEVPRIVATE, &ifr);
#else
    ifr.ifr_ifindex = ifindex;
    ret = ioctl(ctlfd, SIOCBRADDIF, &ifr);
#endif
    if (ret == -1)
    {
        fprintf(stderr, "failed to add interface `%s' to bridge `%s': %s\n", iface, bridge, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* bring the interface up */
    prep_ifreq(&ifr, iface);
    if (ioctl(ctlfd, SIOCGIFFLAGS, &ifr) == -1)
    {
        fprintf(stderr, "failed to get interface flags for `%s': %s\n", iface, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    ifr.ifr_flags |= IFF_UP;
    if (ioctl(ctlfd, SIOCSIFFLAGS, &ifr) == -1)
    {
        fprintf(stderr, "failed to bring up interface `%s': %s\n", iface, strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* write fd to the domain socket */
    if (send_fd(unixfd, fd) == -1)
    {
        fprintf(stderr, "failed to write fd to unix socket: %s\n", strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* ... */

    /* profit! */

cleanup:
    if (fd >= 0)
    {
        close(fd);
    }
    if (ctlfd >= 0)
    {
        close(ctlfd);
    }

    return ret;
}

int main(int argc, char** argv)
{
    int use_vnet = 0;
    const char* bridge = NULL;
    int unixfd = -1;
    int index;

    for (index = 1; index < argc; index++)
    {
        if (strcmp(argv[index], "--use-vnet") == 0)
        {
            use_vnet = 1;
        }
        else if (strncmp(argv[index], "--br=", 5) == 0)
        {
            bridge = &argv[index][5];
        }
        else if (strncmp(argv[index], "--fd=", 5) == 0)
        {
            unixfd = atoi(&argv[index][5]);
        }
        else
        {
            usage();
            return EXIT_FAILURE;
        }
    }

    return bridge_helper(bridge, unixfd, use_vnet);
}
