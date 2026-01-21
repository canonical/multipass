(how-to-guides-troubleshoot-troubleshoot-networking)=
# Troubleshoot networking

<!-- This document combines:
- https://discourse.ubuntu.com/t/how-to-troubleshoot-networking-on-macos/12901
- https://discourse.ubuntu.com/t/how-to-troubleshoot-networking-on-windows/13694
-->

This document demonstrates how to troubleshoot various known Multipass networking issues on macOS and Windows.

## Troubleshoot networking on macOS

### Architecture

On macOS, the QEMU driver employs the Hypervisor.framework. This framework manages the networking stack for the instances.

On creation of an instance, the Hypervisor framework on the host uses macOS's **Internet Sharing** mechanism to:

1. Create a virtual switch and connect each instance to it (subnet 192.168.64.*).
2. Provide DHCP and DNS resolution on this switch at 192.168.64.1 (via `bootpd` & `mDNSResponder` services running on the host). This is configured by an auto-generated file (`/etc/bootpd.plist`), but editing this is pointless as macOS regenerates it as it desires.

Note that, according to **System Preferences > Sharing**, the **Internet Sharing** service can appear disabled. This is fine---in the background, it will still be enabled to support instances.

### Tools known to interfere with Multipass

* VPN software can be aggressive at managing routes and may route 192.168.64 subnet through the VPN interface, instead of keeping it locally available.
    * Possible culprits: OpenVPN, F5, Dell SonicWall, Cisco AnyConnect, Citrix/Netscaler Gateway, Jupiter Junos Pulse / Pulse Secure.
    * `Tunnelblick` doesn’t cause problems.
* Cisco Umbrella Roaming Client it binds to localhost:53 which clashes with Internet Sharing, breaking the instance’s DNS.
<!-- THIS LINK IS BROKEN
(see [Umbrella Roaming Client OS X and Internet Sharing](https://support.umbrella.com/hc/en-us/articles/230561007-Umbrella-Roaming-Client-OS-X-and-Internet-Sharing)) -->
* dnscrypt-proxy/dnscrypt-wrapper/cloudflared-proxy \
The default configuration binds to localhost port 53, clashing with Internet Sharing.
* Another `dnsmasq` process bound to localhost port 53
* Custom DHCP server bound to port 67? (`sudo lsof -iUDP:67 -n -P` should show `launchd` & `bootpd` only)
* macOS updates can make changes to the firewall and leave instances in unknown state; see {ref}`troubleshoot-networking-issues-caused-by-macos-update` below.

### Problem class

* `multipass launch` fails
  * see {ref}`troubleshoot-networking-generic-networking-problems`
* `multipass shell <instance>` fails
  * see {ref}`troubleshoot-networking-routing-problems`
* `multipass shell <instance>` works but the instance cannot connect to the internet
  * see {ref}`troubleshoot-networking-dns-problems`
* extra IPs not reachable between instances
  * see {ref}`troubleshoot-networking-arp-problems`

(troubleshoot-networking-generic-networking-problems)=
#### Generic networking problems

`Unable to determine IP address` usually implies some networking configuration is incompatible, or there is interference from a Firewall or VPN.

> See also: [How to troubleshoot launch/start issues](/how-to-guides/troubleshoot/troubleshoot-launch-start-issues).

##### Troubleshooting

1. Firewall
    1. Is Firewall enabled?
    1. If so it must not "Block all incoming connections"
       * Blocking all incoming connections prevents a DHCP server from running locally, to give an IP to the instance.
       * It’s OK to block incoming connections to `"multipassd"` however.
1. VPN
1. Little Snitch - defaults are good, it should permit `mDNSResponder` and `bootpd` access to BPF
If you're having trouble downloading images and/or see `Unknown error`s when trying to `multipass launch -vvv`, Little Snitch may be interfering with `multipassd`'s network access (ref. [#1169](https://github.com/canonical/multipass/issues/1169))
1. Internet Sharing - doesn’t usually clash

(troubleshoot-networking-routing-problems)=
#### Network routing problems

You could try:

```{code-block} text
sudo route -nv add -net 192.168.64.0/24 -interface bridge100
```

If you get a "File exists" error, maybe delete and retry?

```{code-block} text
sudo route -nv delete -net 192.168.64.0/24
sudo route -nv add -net 192.168.64.0/24 -interface bridge100
```

Maybe `-static` route helps?

If using Cisco AnyConnect, try using OpenConnect (`brew install openconnect`) instead as it messes with routes less (but your company sysadmin/policy may not permit/authorise this).

*   It monitors the routing table so may prevent any customisation. Here is [a very handy but non-standard workaround](https://unix.stackexchange.com/questions/106304/route-add-no-longer-works-when-i-connected-to-vpn-via-cisco-anyconnect-client/501094#501094).

Does your VPN software provide a "split connection" option, where VPN sysadmin can designate a range of IP addresses to **not** be routed through the VPN?
*   Cisco does
*   Pulse Secure / Jupiter Junos Pulse do

#### Potential workaround for VPN conflicts

This was reported on GitHub (issue [#495](https://github.com/canonical/multipass/issues/495#issuecomment-448461250)).

After the `nat ...` line (if there is one, otherwise at the end) in `/etc/pf.conf`, add this line:

```{code-block} text
nat on utun1 from bridge100:network to any -> (utun1)
```

and reload PF with the command:

```{code-block} text
sudo pfctl -f /etc/pf.conf
```

#### Configure Multipass to use a different subnet

Edit `/Library/Preferences/SystemConfiguration/com.apple.vmnet.plist` to change the `"Shared_Net_Address"` value to something other than `192.168.64.1  -`.
*   it works if you edit the `plist` file and stay inside 192.168 range, as Multipass hard-coded for this

```{note}
If you change the subnet and launch an instance, it will get an IP from that new subnet. But if you try changing it back, the change is reverted on next instance start. It appears that the DHCP server reads the last IP in `/var/db/dhcpd_leases`, decides the subnet from that, and updates `Shared_Net_Address` to match. So, the only way to really revert this change is to edit or delete `/var/db/dhcpd_leases`.
```

(troubleshoot-networking-dns-problems)=
#### DNS problems

Can you ping IP addresses?

```{code-block} text
ping 1.1.1.1
```

If not, the output will be similar to the following:

```{code-block} text
PING 1.1.1.1 (1.1.1.1) 56(84) bytes of data.
^C
--- 1.1.1.1 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss, time 2030ms
```

Note that macOS's firewall can block the ICMP packets that `ping` uses, which will interfere with this test. Make sure you disable **Stealth Mode** in **System Preferences > Security & Privacy > Firewall** just for this test.

```{figure} /images/multipass-security-privacy.jpg
   :width: 690px
   :alt: Security & Privacy
```

<!-- Original image on the Asset Manager
![Security & Privacy|690x605](https://assets.ubuntu.com/v1/a4c00e5f-multipass-security-privacy.jpg)
-->

If you try again, it should work:

```{code-block} text
ping 1.1.1.1
```

The output will be similar to the following:

```{code-block} text
PING 1.1.1.1 (1.1.1.1) 56(84) bytes of data.
64 bytes from 1.1.1.1: icmp_seq=1 ttl=53 time=7.02 ms
64 bytes from 1.1.1.1: icmp_seq=2 ttl=53 time=5.91 ms
64 bytes from 1.1.1.1: icmp_seq=3 ttl=53 time=5.12 ms
^C
--- 1.1.1.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2143ms
rtt min/avg/max/mdev = 5.124/6.020/7.022/0.781 ms
```

This means the instance can indeed connect to the internet, but DNS resolution is broken. You can test DNS resolution using the `dig` tool:

```{code-block} text
dig @192.168.64.1 google.ie
```

If broken, the output will be similar to:

```{code-block} text
; <<>> DiG 9.10.3-P4-Ubuntu <<>> google.ie
;; global options: +cmd
;; connection timed out; no servers could be reached
```

On the other hand, if it works correctly the output will be similar to:

```{code-block} text
; <<>> DiG 9.10.3-P4-Ubuntu <<>> google.ie
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 48163
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 4096
;; QUESTION SECTION:
;google.ie.   		 IN    A

;; ANSWER SECTION:
google.ie.   	 15    IN    A    74.125.193.94

;; Query time: 0 msec
;; SERVER: 192.168.64.1#53(192.168.64.1)
;; WHEN: Thu Aug 01 15:17:04 IST 2019
;; MSG SIZE  rcvd: 54
```

To test further, try supplying an explicit DNS server:

```{code-block} text
dig @1.1.1.1 google.ie
```

If it works correctly, the output will be similar to:

```{code-block} text
; <<>> DiG 9.10.3-P4-Ubuntu <<>> @1.1.1.1 google.ie
; (1 server found)
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 11472
;; flags: qr rd ra; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 1

;; OPT PSEUDOSECTION:
; EDNS: version: 0, flags:; udp: 1452
;; QUESTION SECTION:
;google.ie.   		 IN    A

;; ANSWER SECTION:
google.ie.   	 39    IN    A    74.125.193.94

;; Query time: 6 msec
;; SERVER: 1.1.1.1#53(1.1.1.1)
;; WHEN: Thu Aug 01 15:16:27 IST 2019
;; MSG SIZE  rcvd: 54
```

This implies the problem is with the macOS **Internet Sharing** feature---for some reason, its built-in DNS server is broken.

The built-in DNS server should be `"mDNSResponder"`, which binds to localhost on port 53.

If using Little Snitch or another per-process firewall, ensure `mDNSResponder` can establish outgoing connections. The macOS’s built-in firewall should not interfere with it.

Check what is bound to that port on the host with:

```{code-block} text
sudo lsof -iTCP:53 -iUDP:53 -n -P
```

The sample output below shows the correct state while a instance is running:

```{code-block} text
sudo lsof -iTCP:53 -iUDP:53 -n -P
COMMAND   PID       	USER   FD   TYPE         	DEVICE SIZE/OFF NODE NAME
mDNSRespo 191 _mdnsresponder   17u  IPv4 0xa89d451b9ea11d87  	0t0  UDP *:53
mDNSRespo 191 _mdnsresponder   25u  IPv6 0xa89d451b9ea1203f  	0t0  UDP *:53
mDNSRespo 191 _mdnsresponder   50u  IPv4 0xa89d451b9ea8b8cf  	0t0  TCP *:53 (LISTEN)
mDNSRespo 191 _mdnsresponder   55u  IPv6 0xa89d451b9e2e200f  	0t0  TCP *:53 (LISTEN)
```

If no instance is running (and **Internet Sharing** is disabled in **System Preferences**), the command should return nothing.

Any other command appearing in that output means a process is conflicting with **Internet Sharing** and thus will break DNS in the instance.

##### Possible workarounds

1. Configure DNS inside the instance to use an external working DNS server. Can do so by appending this line to /etc/resolv.conf manually:

    ```{code-block} text
    nameserver 1.1.1.1
    ```

    "1.1.1.1" is a free DNS service provided by CloudFlare, but you can use your own.
2. Use a [custom cloud-init to set /etc/resolv.conf](https://cloudinit.readthedocs.io/en/latest/reference/yaml_examples/resolv_conf.html) for you on first boot.

(troubleshoot-networking-arp-problems)=
#### ARP problems

The macOS bridge by Multipass filters packets so that only the IP address originally assigned to the VM is allowed through. If you add an additional address (e.g. an IP alias) to the VM, the ARP broadcast will get through but the ARP response will be filtered out.

This means that applications that rely on additional IP addresses, such as [metallb](https://metallb.universe.tf/) under [microk8s](https://microk8s.io/), will not work.

(troubleshoot-networking-issues-caused-by-macos-update)=
#### Issues caused by macOS update

When upgrading macOS to 12.4 (this might happen however also when upgrading to other versions), macOS makes changes to the firewall. If the instances are not stopped before the update, it is possible the connection to the instances are blocked by the macOS firewall. We cannot know what is exactly the change introduced to the firewall, it seems the Apple's `bootpd` stops replying DHCP requests.

There are some procedures that can help to overcome this issue (see [issue #2387](https://github.com/canonical/multipass/issues/2387) on the Multipass GitHub repository for a discussion on this and some alternative solutions). First, you can try to:

* Reboot the computer.
* Disable and then re-enable Internet sharing and/or the firewall.
* Configure the driver (QEMU) and Multipass in the firewall to allow incoming connections.

## Troubleshoot networking on Windows

This sections contains troubleshooting considerations that are specific to Windows systems.

### Architecture

Multipass uses the native "Hyper-V" hypervisor on Windows, along with the "Default Switch" created for it. That, in turn, uses the "Internet Sharing" functionality, providing DHCP (IP addresses) and DNS (domain name resolution) to the instances.

### Known issues

Here you can find more details on known issues affecting Windows systems.

#### Default switch going awry

Unfortunately the default switch is known to be quirky and Windows updates often put it in a weird state. This may result in new instances failing to launch and existing ones timing out to start.

The broken state also persists over reboots. The one approach that has helped is removing the network sharing from the default switch:

```{code-block} powershell
Get-HNSNetwork | ? Name -Like "Default Switch" | Remove-HNSNetwork
```

and then rebooting the system:

```{code-block} powershell
Restart-Computer
```

Hyper-V will recreate it on next boot.

<!-- This content has been moved to troubleshoot-launch-start-issues.md
#### Stale Internet connection sharing lease

Another reason for instance timeouts may be that a "stale" IP address for a particular instance name is stored in the `Internet Connection Sharing` hosts file.

Using Administrator privileges, edit the file `C:\WINDOWS\System32\drivers\etc\hosts.ics` and look for any entries that have your instance name in it. If there is more than one entry, remove any of them except for the first listed. Save the file and try again.
-->

#### Anti-virus / security software blocking instances

Anti-virus and network security software are not necessarily virtualisation-aware. If you’re having issues with connectivity, temporarily disabling this software to test can result in a positive outcome. Examples of this software are Symantec, ESET, Kaspersky and Malware Bytes.

<!-- Discourse contributors
<small>**Contributors:** @saviq , @townsend , @sowasred2012 , @ya-bo-ng , @candlerb , @sergiusens , @nhart , @andreitoterman , @tmihoc , @luisp , @ricab , @gzanchi , @naynayu , @QuantumLibet </small>
-->
