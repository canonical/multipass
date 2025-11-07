(how-to-guides-troubleshoot-troubleshoot-launch-start-issues)=
# Troubleshoot launch/start issues

This guide helps you diagnose and fix common Multipass launch or start failures such as *timeouts*, *"unknown state"*, or *SSH connection* errors.

These issues are often related to **networking**, **IP assignment**, **boot problems**, or **SSH access** failures and they may occur for one or more of the following reasons:

- The instance fails to load its required image due to **stale network cache**.
- The virtual machine (VM) fails to **boot properly** and does not reach the stage where it requests an IP address.
- The VM **requests an IP address** but fails to obtain one.
- The VM obtains an IP address, but:
  - Multipass cannot locate it.
  - Multipass locates an IP that does not match the assigned address.
  - SSH fails inside the VM or is blocked by the host system.
- The instance **times out** while waiting for initialisation to complete.

## Before you start

```{important}
Follow these steps to diagnose your issue and identify the most likely scenario:
```

* If the multipass launch command fails with the message “Downloaded image hash does not match”, see [Stale network cache](troubleshoot-launch-start-issues-stale-network-cache).

  `````{tab-set}
    ```{tab-item} Windows, Hyper-V driver
      Inspect the file `C:\WINDOWS\System32\drivers\etc\hosts.ics` and see if there is more than one entry with your instance name in it. If that’s the case, see [[Windows, with Hyper-V] Stale internet connection sharing lease](troubleshoot-launch-start-issues-stale-internet).
    ````
    ````{tab-item} Linux/macOS, QEMU driver: NIC_RX_FILTER_CHANGED
      Inspect the Multipass logs and look for a message mentioning `NIC_RX_FILTER_CHANGED`. This message indicates that the network interface has been initialised.
      - If you don’t find it, it means that the VM didn’t manage to bring up the interface; see [**VM boot failure**](troubleshoot-launch-start-issues-vm-boot-failure).
      - If the message is present, proceed to check DHCP traffic in the next tab.
    ````
    ````{tab-item} Linux/macOS, QEMU driver: DHCP traffic
      Check DHCP traffic from your host to the instance, to find out if there are requests and replies. Adapt and run the following command right after starting/launching the instance:

      ```bash
        sudo tcpdump -i <bridge> udp port 67 and port 68
      ```
      You will need to replace `<bridge>` with `mpqemubr0` on Linux and with `bridge100` on macOS.

      ```{note}
        On macOS, `bridge100` is a virtual network interface that only appears when at least a VM is running.
      ```
      - If you see `NIC_RX_FILTER_CHANGED`, you should also see DHCP requests. If you don’t, see [**VM boot failure**](troubleshoot-launch-start-issues-vm-boot-failure) and please let us know.
      - If you see a DHCP request, but no reply, it means that the VM is still waiting for an IP address to be assigned; see [**No IP assigned**](troubleshoot-launch-start-issues-no-ip-assigned).
      - If you see DHCP requests and replies, continue to the next step.
    ````
  `````

* Look for messages regarding SSH in Multipass’s logs. The instance may have obtained an IP and/or be properly connected, but still refuse Multipass when it tries to SSH into it.

* Look for the message in the CLI or GUI spinner. Once it reads "Waiting for initialisation to complete", Multipass will have succeeded SSH-ing into the instance but remain waiting for cloud-init to finish.


(troubleshoot-launch-start-issues-vm-boot-failure)=
## 1. VM boot failure

**Issue:**
Instance fails to start or gets stuck during boot.

**Cause:**
The VM might not reach the point of requesting an IP address, often due to image corruption or boot configuration problems.

`````{dropdown} Workaround

````{tab-set}
```{tab-item} QEMU (Linux/macOS)
1. Locate the `qemu-system-*` command for the failing VM in Multipass logs.
2. Edit it:
   - Remove `-serial chardev:char0 -nographic`.
   - Escape spaces in paths (`Application Support` → `Application\ Support`).
3. Run the edited command with `sudo`.

Example:
```{code-block} text
/Library/Application\ Support/com.canonical.multipass/bin/qemu-system-aarch64 -machine virt,gic-version=3 -accel hvf -drive file=/Library/Application\ Support/com.canonical.multipass/bin/../Resources/qemu/edk2-aarch64-code.fd,if=pflash,format=raw,readonly=on -cpu host -nic vmnet-shared,model=virtio-net-pci,mac=52:54:00:e2:30:dd -device virtio-scsi-pci,id=scsi0 -drive file=/var/root/Library/Application\ Support/multipassd/qemu/vault/instances/superior-chihuahua/ubuntu-22.04-server-cloudimg-arm64.img,if=none,format=qcow2,discard=unmap,id=hda -device scsi-hd,drive=hda,bus=scsi0.0 -smp 2 -m 4096M -qmp stdio -cdrom /var/root/Library/Application\ Support/multipassd/qemu/vault/instances/superior-chihuahua/cloud-init-config.iso
```

```{tab-item} VirtualBox (macOS/Windows)
1. Run VirtualBox as Administrator:
   - macOS: `sudo VirtualBox`
   - Windows: Use PsExec as shown in {ref}`launch-start-issues-windows-run-virtualbox`
2. Start or launch the instance and attach to the VM to view boot logs.
```
```{tab-item} Hyper-V (Windows)
1. Open **Hyper-V Manager**.
2. Start or launch the instance.
3. Select the VM → **Connect** → observe boot output.
```
````
`````

(troubleshoot-launch-start-issues-vm-image-corruption)=
## 2. VM image corruption

**Issue:**
Instance doesn’t boot even after restarting.

**Cause:**
Corrupted VM disk image, often due to abrupt shutdowns.

````{dropdown} Workaround


- If you have a [snapshot](/explanation/snapshot), restore it.
- Run `fsck` in the VM’s console to repair the filesystem:
  * Access the VM console (as above).
  * Interrupt boot with **Esc** or **Shift** to reach GRUB.
  * Enter **Advanced options for Ubuntu → (recovery mode)**.
  * Select **fsck** and confirm.
  * Optionally, drop to a root shell and run:
     ```bash
     mount -o remount,ro /
     fsck -f /
     mount -o remount,rw /
     exit
     ```
  * Choose **resume** to continue normal boot.
- *(Linux/macOS)* Alternatively, mount and repair the image on host (see {ref}`troubleshoot-launch-start-issues-reading-data-from-an-image`).
- Use `qemu-img check -r` to repair:
  ```bash
  /Library/Application\ Support/com.canonical.multipass/bin/qemu-img check -r /var/root/Library/Application\ Support/multipassd/qemu/vault/instances/<instance>/<img>
  ```
If none of these succeed, manually mount the image and recover data.
````

(troubleshoot-launch-start-issues-no-ip-assigned)=
## 3. No IP assigned

```{seealso}
See also: {ref}`how-to-guides-troubleshoot-troubleshoot-networking`
```

**Issue:**
The instance launches but doesn’t get an IP address.

**Cause:**
DHCP or network interface issues, often due to firewall or VPN interference.

``````{dropdown} Workaround (macOS, QEMU driver)

  `````{tab-set}
  ````{tab-item} Firewall blocks bootp
    The macOS firewall can block Apple’s bootp, preventing IP assignment.

    Run:
    ```bash
    /usr/libexec/ApplicationFirewall/socketfilterfw --add /usr/libexec/bootpd
    /usr/libexec/ApplicationFirewall/socketfilterfw --unblock /usr/libexec/bootpd
    ```
    If using a managed Mac (no admin rights), report this to Apple — the issue affects all vmnet-based products. Tracked in GitHub issue [#2387](https://github.com/canonical/multipass/issues/2387).
  ````
  ````{tab-item} bootp not starting
    If DHCP doesn’t start automatically, run:
    ```bash
    sudo launchctl start com.apple.bootpd
    ```
    If that fails:
    ```bash
    sudo launchctl load -w /System/Library/LaunchDaemons/bootps.plist
    ```
  ````
  `````
``````

(troubleshoot-launch-start-issues-stale-internet)=
## 4. Stale Internet Connection Sharing lease (Windows, Hyper-V)

**Issue:**
Timeout or “unknown state” when launching instances.

**Cause:**
Duplicate or corrupted entries in the hosts.ics file.

```{dropdown} Workaround


* Edit `C:\WINDOWS\System32\drivers\etc\hosts.ics` as Administrator.
* Remove duplicate or corrupt entries for your instance.
* If issue persists:
   - Stop all instances.
   - Delete hosts.ics.
   - Reboot the system.

```

(troubleshoot-launch-start-issues-ssh-issues)=
## 5. SSH issues

**Issue:**
Instance has an IP, but Multipass can’t SSH into it.

**Cause:**
Incorrect SSH configuration, wrong username, or missing keys.

````{dropdown} Workaround

* Ensure default user is `ubuntu`.
* If SSH keys are missing, copy host’s key to instance:
  ```bash
  cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
  ```
  (access the instance manually or by mounting its image if SSH fails).
* Gain access without SSH:
  - Mount the instance’s image (see {ref}`troubleshoot-launch-start-issues-reading-data-from-an-image`).
  - Run the VM directly and log in as:
    - username: `ubuntu`
    - password: `ubuntu`
* To reset password:
  * Backup the existing cloud-init-config.iso.
  * Launch a new instance with:
     ```bash
     multipass launch --cloud-init config.yaml
     ```
  * Example `config.yaml`:
     ```yaml
     #cloud-config
     password: ubuntu
     chpasswd: { expire: false }
     ```
  * Replace the old ISO with the new one and restart the VM.
````

(troubleshoot-launch-start-issues-cloud-init)=
## 6. Cloud-init takes too long

**Issue:**
multipass launch hangs with “Waiting for initialisation to complete”.

**Cause:**
Cloud-init is still configuring the instance in the background.

````{dropdown} Workaround

- Wait longer; setup may still complete.
- Check `/var/lib/cloud/instance/boot-finished` inside the instance.
- To increase timeout:
  ```bash
  multipass launch --timeout 1000
  ```
- Inspect logs in `/var/log/cloud-init*log` for progress or errors.
````

(troubleshoot-launch-start-issues-stale-network-cache)=
## 7. Stale network cache

**Issue:**
multipass launch fails with “Downloaded image hash does not match”.

**Cause:**

This can be caused by a known Qt bug (see issue [#1714](https://github.com/canonical/multipass/issues/1714) on our GitHub).

``````{dropdown} Workaround

* Run:
  ```bash
  multipass find --force-update
  ```
* If unresolved, clear the cache manually and restart Multipass.

`````{tab-set}
  ````{tab-item} Linux
    ```bash
    sudo snap stop multipass
    sudo rm -rf /var/snap/multipass/common/cache/multipassd/network-cache/
    sudo snap start multipass
    ```
  ````
  ````{tab-item} macOS
    ```bash
    sudo launchctl unload /Library/LaunchDaemons/com.canonical.multipassd.plist
    sudo rm -rf /System/Volumes/Data/private/var/root/Library/Caches/multipassd/network-cache
    sudo launchctl load /Library/LaunchDaemons/com.canonical.multipassd.plist
    ```
  ````
  ````{tab-item} Windows
    Remove `C:\ProgramData\Multipass\cache\network-cache` and restart the service.
  ````
  `````
``````


## Appendix: Utility commands and locations

(launch-start-issues-locate-multipass-binaries)=
### Locating Multipass binaries

You may need to locate where Multipass is installed. Use the appropriate tab for your platform:

`````{tab-set}
  ````{tab-item} Linux
    Run one of the following commands to find the Multipass binary:
    ```bash
    which multipass
    whereis multipass
    ```
    By default, Multipass is installed in the `/snap/bin` folder.
  ````
  ````{tab-item} Windows
    Run the following command to find the Multipass binary:
    ```powershell
    where.exe multipass
    ```
    Or, right-click a shortcut to Multipass in your files or Start menu and select "Open file location".
    By default, Multipass is installed in the `C:\\Program Files\\Multipass\\bin` folder.
  ````
  ````{tab-item} macOS
    Run the following command to find the Multipass binary:
    ```bash
    readlink -f $(which multipass)
    ```
    By default, Multipass is installed in the `/Library/Application\ Support/com.canonical.multipass/bin/` folder.
  ````
`````

(launch-start-issues-locate-multipass-images)=
### Locating Multipass images

You may need to locate where Multipass is storing instances. Use the appropriate tab for your platform:

`````{tab-set}
  ````{tab-item} Linux
    The default location is:
    ```text
    /root/.local/share/multipassd/vault/instances/<instance>/<img>
    ```
  ````
  ````{tab-item} Windows
    The default location is:
    ```text
    C:\ProgramData\Multipass\data\vault\instances\<instance>\<img>
    ```
  ````
  ````{tab-item} macOS
    The default location is:
    ```text
    /var/root/Library/Application\ Support/multipassd/qemu/vault/instances/<instance>/<img>
    ```
  ````
`````

(troubleshoot-launch-start-issues-reading-data-from-an-image)=
### Reading data from a QCOW2 image

Multipass QEMU driver images use QCOW2 format, which other tools can read.

One example is [`qemu-nbd`](https://manpages.ubuntu.com/manpages/questing/en/man8/qemu-nbd.8.html), which allows mounting the image. This tool is not shipped with Multipass, so you would need to install it manually.

Once you have it, you can search the web for recipes to mount a QCOW2 image. For example, here is a [a recipe](https://askubuntu.com/a/4404).

(launch-start-issues-windows-run-virtualbox)=
### Running VirtualBox with the system account (Windows, VirtualBox driver)

To run the VirtualBox GUI with the system account, you will need a Windows tool called PsExec:

1. Install [PsExec](https://docs.microsoft.com/en-us/sysinternals/downloads/psexec).
1. Add it to your `PATH`.
1. Run PowerShell as Administrator.
1. Execute `psexec.exe -s -i "C:\Program Files\Oracle\VirtualBox\VirtualBox.exe"` (adapt the path to the VirtualBox executable as needed).

When successful, you should see Multipass's instances in VirtualBox
---

## Get support

If these troubleshooting steps do not resolve your issue:

- [Open a new issue on GitHub](https://github.com/canonical/multipass/issues/new/choose)
  - Include:
    - The full output of the failing command.
    - Relevant logs from `multipassd`.
    - Your OS, Multipass version, and driver used.
- Or visit the [Multipass Discourse Forum](https://discourse.ubuntu.com/c/project/multipass/21/none) for community help.
