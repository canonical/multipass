# Running Multipass on a Windows guest using libvirt/QEMU

_Note: The original document was written in 2020 and might be outdated._

A guest running on qemu/libvirt on Linux will not expose as default the features needed by Windows to enable Hyper-V virtualization.

There are three requirements to run Hyper-V on a Windows Pro, Enterprise or Education (not Home) guest:
1. nested virtualization must be enabled in the host;
2. the host must expose some features to the guest;
3. virtualization must be enabled in the guest.

## Host configuration
Intel processors support nested virtualization starting in the Haswell series; some AMD processors also support it.

### KVM configuration
The nesting should be enabled on KVM, through the `kvm-intel` or `kvm-amd` modules. For this, the options `nested` and `ept` (Extended Page Tables, see [here](https://en.wikipedia.org/wiki/Second_Level_Address_Translation#EPT)) must be enabled. They are enabled by default but, if not, they can be forced at boot time adding the file `/etc/modprobe.d/kvm-nested.conf` containing the lines
```
options kvm-intel nested=1
options kvm-intel ept=1
```

[Here](https://www.linuxtechi.com/enable-nested-virtualization-kvm-centos-7-rhel-7/) it is suggested to also activate the options `enable_shadow_vmcs` and `enable_apicv`. Although they are not needed, they may be important. Unfortunately, I can't test because my Broadwell doesn't support them. Testing them with newer hardware is welcome.

The procedure must be analogous for the kvm-amd module, though I didn't test it because I don't have an AMD at hand. There is an option called `nested` and the analogous to `ept` should be `npt` (Nested Page Tables, a.k.a. Rapid Virtualization Indexing, see [here](https://en.wikipedia.org/wiki/Second_Level_Address_Translation#Rapid_Virtualization_Indexing)).

The analogous in AMD to the module option `enable_apicv` is `avic`. There should not exist an AMD analogous to `enable_shadow_vmcs`, because shadowing VMCS is an alias for nested virtualization.

## QEMU configuration
On Intel, it is mandatory to expose to the guest the `vmx` feature (which indicates that the processor supports VT-x). On AMD, it is mandatory to expose the `svm` feature. Also disabling the `xsaves` feature is needed on some Intel and AMD processors, to work around [this bug](https://bugs.launchpad.net/qemu/+bug/1864536) on CPUs that have `xsaves` (can be checked with `virsh capabilities`). All this can be accomplished using command-line QEMU or through libvirt.

### Through libvirt
In the `<cpu>` section of the virtual machine's XML, adding the lines
```xml
<feature policy="require" name="vmx"/>
<feature policy="disable" name="xsaves"/>
```
will do. An example of this follows.
```xml
  <cpu mode="host-model" check="partial">
    <feature policy="require" name="vmx"/>
    <feature policy="disable" name="xsaves"/>
  </cpu>
```

On AMD, virtualization is indicated by the `svm` feature. We have to expose this, using something like
```xml
<feature policy="require" name="svm"/>
<feature policy="disable" name="xsaves"/>
```

### Using command-line qemu
On Intel, the `cpu` option must include the feature options `+vmx` (as suggested [here](https://ladipro.wordpress.com/2017/02/24/running-hyperv-in-kvm-guest/)) and `-xsaves`.

On AMD, the `+svm` feature must be included in the command line. Some AMD processors also support `xsaves`, so the `-xsaves` option must be passed.

## Enabling Hyper-V in the guest
You can follow one of the methods described [here](https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/quick-start/enable-hyper-v).

## TODO
### Improve performance by exposing more processor features
It was reported that enabling nesting makes the guest slow. However, some processor features which are not exposed automatically can be exposed by hand (like SSE registers, AVIC/APICV, ...). Some investigation must include executing in the host and in a guest Linux `cat /proc/cpuinfo | grep flags | grep uniq` and compare which flags are missing in the guest, and evaluate if they improve guest performance when manually exposed.

### Test on AMD
All what is written on this page about AMD was not tested. We need to test the `kvm-amd` module parameters, as well as qemu options and if all of this makes Hyper-V work on the guest Windows.

## Other useful links
* Wikipedia entry about x86 virtualization: https://en.wikipedia.org/wiki/X86_virtualization
* Description of the XML format used by libvirt: https://libvirt.org/formatdomain.html
* On qemu hyper-v enlightenments: https://github.com/qemu/qemu/blob/master/docs/hyperv.txt
* About enlightenments, more human-friendly: https://scottlinux.com/2016/03/21/enable-hyper-v-enlightenments-in-kvm-for-better-windows-vm-performance/
* Answer about nesting, which proposed a working solution: https://serverfault.com/a/628133
