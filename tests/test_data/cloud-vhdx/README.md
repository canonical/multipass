This folder contains a lightweight virtual machine disk image (alpine linux) for the integration tests.

The disk is split into 95MB chunks since GitHub's non-lfs file max size is 100MB.

```
split -b 95M alpine.vhdx alpine.vhdx.part-
```

Configuration: NoCloud-3.20.8-x86_64-UEFI-cloud-init-virtual