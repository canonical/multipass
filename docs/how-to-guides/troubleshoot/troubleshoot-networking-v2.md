(how-to-guides-troubleshoot-troubleshoot-networking-v2)=
# Troubleshoot networking

This guide helps you troubleshoot known Multipass networking issues on macOS and Windows.


## Before you start

Before troubleshooting a specific symptom, review these quick checks:

- [Apps that commonly interfere with Multipass](#tn2-interfering-apps).
- [How Multipass networking works](#tn2-networking-background), if you need more context.

## Which problem do you have?

The following scenarios describe commonly encountered Multipass networking problems. Choose the one that matches what you see.

### macOS

- [An instance won't start, and you see `Unable to determine IP address`](#tn2-macos-launch).
- [`multipass shell` doesn't respond or fails to connect](#tn2-macos-routing).
- [Your instance starts, but it can't connect to the internet](#tn2-macos-dns).
- [Extra IP addresses aren't reachable between instances](#tn2-macos-arp).
- [Networking stopped working right after a macOS update](#tn2-macos-update).

### Windows

- [Instances won't start or keep timing out](#tn2-windows-switch).
- [Connectivity is unreliable and you run anti-virus or security software](#tn2-windows-av).
- [Upload speeds over Wi-Fi are very slow](#tn2-windows-wifi).

---

(tn2-interfering-apps)=
## Apps that commonly interfere

Before troubleshooting a specific symptom, check whether you're running any of the following. These are the most common cause of Multipass networking problems, and quitting them often fixes the issue outright:

- **VPN software** can redirect Multipass's private network range through the VPN instead of keeping it on your computer. Known culprits: OpenVPN, F5, Dell SonicWall, Cisco AnyConnect, Citrix/Netscaler Gateway, and Juniper Junos Pulse / Pulse Secure. (Tunnelblick is known to be fine.)
- **Cisco Umbrella Roaming Client** takes over the name-resolution port (port 53) and breaks your instance's DNS.
- **dnscrypt-proxy, dnscrypt-wrapper, or cloudflared-proxy** do the same by default.
- **A second `dnsmasq` process**, or a **custom DHCP server**, can also clash. On macOS, check what is using the DHCP port with `sudo lsof -iUDP:67 -n -P`; you should only see `launchd` and `bootpd`.

If you use any of these, try quitting it and reproducing your problem before going further.

---

## On macOS

(tn2-macos-launch)=
### An instance won't start on macOS

**Problem**

> I try running `multipass launch` and it fails. The error mentions it can't determine an IP address

**What you'll see**

A launch that fails with a message containing:

```{code-block} text
Unable to determine IP address
```

**What's happening**

This usually means some networking configuration is incompatible, or there is interference from a firewall or VPN.

```{seealso}
[How to troubleshoot launch/start issues](/how-to-guides/troubleshoot/troubleshoot-launch-start-issues).
```

**How to fix it**

Work through these one at a time, trying to launch again after each:

1. **Check your firewall.** Open **System Preferences > Security & Privacy > Firewall**. The firewall can be on, but it must **not** be set to "Block all incoming connections", which stops the local service that gives your instance an address. (It is fine to block incoming connections specifically to `multipassd`.)
2. **Check your VPN.** If you use a VPN, disconnect it and try again. See [Apps that are known to interfere](#tn2-interfering-apps).
3. **Check Little Snitch** (or any similar per-app firewall). Its defaults are usually fine, but make sure it allows `mDNSResponder` and `bootpd`. If image downloads fail or you see `Unknown error` when running `multipass launch -vvv`, Little Snitch may be blocking `multipassd`'s network access (see [issue #1169](https://github.com/canonical/multipass/issues/1169)).

(tn2-macos-routing)=
### You can't open a shell in your instance

> The instance is running, but `multipass shell` doesn't respond or won't connect.

**What you'll see**

```{code-block} text
observation to be inserted later
```

**What's happening**

Your computer can't route traffic to the instance's network. This is most often caused by VPN software that takes control of the routing table.

**How to fix it**

1. Add a route to the instance network by hand:

    ```{code-block} text
    sudo route -nv add -net 192.168.64.0/24 -interface bridge100
    ```

2. If you get a `File exists` error, delete the existing route first and add it again:

    ```{code-block} text
    sudo route -nv delete -net 192.168.64.0/24
    sudo route -nv add -net 192.168.64.0/24 -interface bridge100
    ```

If you use **Cisco AnyConnect**, it actively monitors the routing table and may undo your changes. Two options:

- Use **OpenConnect** instead (`brew install openconnect`), which interferes with routes far less. Check with your IT department first, as your company policy may not allow it. There is also a [non-standard workaround](https://unix.stackexchange.com/questions/106304/route-add-no-longer-works-when-i-connected-to-vpn-via-cisco-anyconnect-client/501094#501094) if you must stay on AnyConnect.
- Ask whether your VPN offers a **split tunnel** option, where your VPN administrator can mark a range of addresses to be excluded from the VPN. Cisco and Pulse Secure / Juniper Junos Pulse both support this.

#### A workaround for VPN conflicts

This was reported in [issue #495](https://github.com/canonical/multipass/issues/495#issuecomment-448461250). Add the following line to `/etc/pf.conf`, after the `nat ...` line if there is one, otherwise at the end:

```{code-block} text
nat on utun1 from bridge100:network to any -> (utun1)
```

Then reload the firewall rules:

```{code-block} text
sudo pfctl -f /etc/pf.conf
```

#### Use a different network range

If the `192.168.64.*` range clashes with your network, you can change it. Edit `/Library/Preferences/SystemConfiguration/com.apple.vmnet.plist` and change the `Shared_Net_Address` value. Stay within the `192.168.*` range, as Multipass relies on it.

```{note}
If you change the range and launch an instance, it will get an address from the new range. Changing it back, however, is reverted the next time an instance starts: the DHCP service reads the last address in `/var/db/dhcpd_leases`, works out the range from it, and resets `Shared_Net_Address` to match. To truly revert the change, edit or delete `/var/db/dhcpd_leases`.
```

(tn2-macos-dns)=
### Your instance can't connect to the internet

> I can open a shell in my instance, but it can't connect to the internet.

**Step 1: Can the instance reach the internet at all?**

Inside the instance, try to reach an address by its numbers:

```{code-block} text
ping 1.1.1.1
```

If there's no connection, you'll see every packet lost:

```{code-block} text
PING 1.1.1.1 (1.1.1.1) 56(84) bytes of data.
^C
--- 1.1.1.1 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss, time 2030ms
```

```{note}
The macOS firewall can block the test messages that `ping` uses, which makes this test misleading. Just for this test, turn off **Stealth Mode** in **System Preferences > Security & Privacy > Firewall**.
```

```{figure} /images/multipass-security-privacy.jpg
   :width: 690px
   :alt: Security & Privacy
```

With Stealth Mode off, a working connection looks like this:

```{code-block} text
PING 1.1.1.1 (1.1.1.1) 56(84) bytes of data.
64 bytes from 1.1.1.1: icmp_seq=1 ttl=53 time=7.02 ms
64 bytes from 1.1.1.1: icmp_seq=2 ttl=53 time=5.91 ms
64 bytes from 1.1.1.1: icmp_seq=3 ttl=53 time=5.12 ms
^C
--- 1.1.1.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2143ms
```

If this works, the instance can reach the internet, but **DNS resolution** is broken. Continue below.

**Step 2: Is name resolution broken?**

Still inside the instance, ask the built-in resolver to look up a name:

```{code-block} text
dig @192.168.64.1 google.ie
```

If it's broken, the request times out:

```{code-block} text
; <<>> DiG 9.10.3-P4-Ubuntu <<>> google.ie
;; global options: +cmd
;; connection timed out; no servers could be reached
```

To confirm the problem is the built-in resolver and not the wider internet, ask a public resolver directly:

```{code-block} text
dig @1.1.1.1 google.ie
```

If that one succeeds (you get an `ANSWER SECTION` with an address), then your instance's connection is fine and the macOS **Internet Sharing** name resolver is the culprit.

**What's happening**

The built-in resolver, `mDNSResponder`, listens on port 53. If another program has taken that port, name resolution inside your instance breaks.

**How to fix it**

1. If you use Little Snitch or a similar per-app firewall, make sure `mDNSResponder` is allowed to make outgoing connections. The built-in macOS firewall should not block it.
2. Check what is using port 53 on your computer:

    ```{code-block} text
    sudo lsof -iTCP:53 -iUDP:53 -n -P
    ```

    While an instance is running, you should see **only** `mDNSResponder`:

    ```{code-block} text
    COMMAND   PID       	USER   FD   TYPE         	DEVICE SIZE/OFF NODE NAME
    mDNSRespo 191 _mdnsresponder   17u  IPv4 0xa89d451b9ea11d87  	0t0  UDP *:53
    mDNSRespo 191 _mdnsresponder   25u  IPv6 0xa89d451b9ea1203f  	0t0  UDP *:53
    mDNSRespo 191 _mdnsresponder   50u  IPv4 0xa89d451b9ea8b8cf  	0t0  TCP *:53 (LISTEN)
    mDNSRespo 191 _mdnsresponder   55u  IPv6 0xa89d451b9e2e200f  	0t0  TCP *:53 (LISTEN)
    ```

    If no instance is running and Internet Sharing is off, the command returns nothing. **Any other program in this list** is conflicting with Internet Sharing and breaking your instance's DNS; quit it. See [Apps that are known to interfere](#tn2-interfering-apps).

**If you can't remove the conflicting program**

You can point the instance at a public resolver instead:

1. Inside the instance, add this line to `/etc/resolv.conf`:

    ```{code-block} text
    nameserver 1.1.1.1
    ```

    (`1.1.1.1` is a free resolver from Cloudflare; use any you prefer.)
2. To make this automatic on every new instance, use a [custom cloud-init file that sets `/etc/resolv.conf`](https://cloudinit.readthedocs.io/en/latest/reference/yaml_examples/resolv_conf.html) at first boot.

(tn2-macos-arp)=
### Extra IP addresses aren't reachable

> I added an extra IP address to my instance, but nothing on the network can reach it.

**What's happening**

On macOS, the Multipass network only allows through the single IP address it originally assigned to each instance. If you add another address (an "IP alias"), requests for it get out but the replies are filtered, so the address never answers.

This means tools that depend on extra IP addresses, such as [MetalLB](https://metallb.universe.tf/) under [MicroK8s](https://microk8s.io/), won't work on macOS.

(tn2-macos-update)=
### Networking stopped after a macOS update

> Everything worked before. I updated macOS, and now my instances can't connect.

**What's happening**

A macOS update (this was first seen with 12.4, but can happen on other versions too) changes the firewall. If instances were still running during the update, the firewall can end up blocking them: Apple's `bootpd` service appears to stop answering address requests.

**How to fix it**

Try these in order (see [issue #2387](https://github.com/canonical/multipass/issues/2387) for the full discussion):

1. Restart your computer.
2. Turn Internet Sharing and/or the firewall off and then on again.
3. In the firewall settings, allow incoming connections for the driver (QEMU) and for Multipass.

---

## On Windows

(tn2-windows-switch)=
### Instances won't start or keep timing out

> New instances fail to launch, or existing ones hang and time out when starting, and the problem survives a reboot.

**What you'll see**

```{code-block} text
observation to be inserted later
```

**What's happening**

The Hyper-V Default Switch is known to be unreliable, and Windows updates often leave it in a broken state. Because the broken state persists across reboots, restarting alone won't fix it.

**How to fix it**

Reset the Default Switch by removing its network sharing. In a PowerShell window with administrator rights:

```{code-block} powershell
Get-HNSNetwork | ? Name -Like "Default Switch" | Remove-HNSNetwork
```

Then restart your computer:

```{code-block} powershell
Restart-Computer
```

Hyper-V recreates the switch automatically on the next boot.

(tn2-windows-av)=
### Your security software is blocking instances

> My connectivity is unreliable, and I run anti-virus or network security software.

**What's happening**

Anti-virus and network security tools aren't always aware of virtual machines and can block their traffic. Known examples include Symantec, ESET, Kaspersky, and Malwarebytes.

**How to fix it**

If you're having connectivity issues, temporarily disabling this software to test can result in a positive outcome.

(tn2-windows-wifi)=
### Slow Wi-Fi upload speeds

> I created an External Switch on my Wi-Fi adapter in Hyper-V, and now my upload speed has dropped to around 1 Mbit/s.

**What's happening**

This is a long-standing Windows networking limitation: certain network "offload" features interfere with Hyper-V's virtual networking and cripple upload speed. The fix is to switch off those features for the virtual adapter.

**How to fix it**

1. Open **Device Manager**.
2. Expand **Network Adapters**.
3. Find **Hyper-V Virtual Ethernet Adapter #N** (where *N* is the one connected to Wi-Fi).
4. Right-click it and choose **Properties**.
5. Go to the **Advanced** tab.
6. Find and disable both of these:
   - **Large Send Offload v2 (IPv4)**
   - **Large Send Offload v2 (IPv6)**
7. Click **OK**, then restart your networking or your machine if needed.

---

(tn2-networking-background)=
## How Multipass networking works

This background information can help if you need to understand why the previous troubleshooting steps mention specific host services, address ranges, or switches.

(tn2-macos-networking-background)=
### macOS

On macOS, Multipass uses Apple's built-in **Internet Sharing** feature to give your instances a network. When you create an instance, macOS:

1. Creates a private network and connects each instance to it, using the address range `192.168.64.*`.
2. Hands out IP addresses and resolves names on that network from `192.168.64.1`, using Apple's own `bootpd` and `mDNSResponder` services.

In **System Preferences > Sharing**, **Internet Sharing** may appear switched off. That's normal; it still runs in the background for your instances.

(tn2-windows-networking-background)=
### Windows

On Windows, Multipass uses the built-in **Hyper-V** virtualization platform and its **Default Switch**. That switch uses Windows **Internet Sharing** to give your instances their IP addresses and name resolution.
