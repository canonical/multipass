# Building and using a locally built QEMU

Notes:
- QEMU comes via vcpkg by default
- This document might be outdated in some parts.

## 1 - Install dependencies

This is what I ran:

```sh
sudo apt install libfdt-dev zlib1g-dev libsdl2-dev libgtk-3-dev \
    libvte-dev libcapstone-dev libattr1-dev libcap-ng-dev \
    libglib2.0-dev libpixman-1-dev libseccomp-dev meson libnfs-dev \
    libiscsi-dev
```

Not sure what is actually needed, and most of these I had already.

References:
  - https://wiki.qemu.org/Hosts/Linux
  - `build-packages` for qemu in our `snapcraft.yaml`

## 2 - Get the source

Clone our repo and checkout the branch:

```sh
git clone https://github.com/canonical/qemu.git can-qemu
cd can-qemu
git checkout -t origin/multipass-8.0.0+9p-uid-gid-map
```

Look for the current branch in our `snapcraft.yaml`.

References:
  - `source-branch` for qemu in our `snapcraft.yaml`

## 3 - Configure the build

```sh
mkdir build
cd build
../configure \
  --enable-virtfs \
  --disable-bochs \
  --disable-cloop \
  --disable-docs \
  --disable-guest-agent \
  --disable-parallels \
  --disable-qed \
  --disable-libiscsi \
  --disable-vnc \
  --disable-xen \
  --disable-dmg \
  --disable-replication \
  --disable-hax \
  --disable-snappy \
  --disable-lzo \
  --disable-live-block-migration \
  --disable-vvfat \
  --disable-curl \
  --disable-tests \
  --disable-nettle \
  --disable-libusb \
  --disable-bzip2 \
  --disable-gcrypt \
  --disable-gnutls \
  --disable-slirp \
  --disable-user \
  --disable-libvduse \
  --disable-vduse-blk-export \
  --enable-strip \
  --firmwarepath=share/qemu:/usr/share/qemu \
  --target-list=x86_64-softmmu
```

The last option — `--target-list` — specifies that we want to build only for running system emulation on `x86_64`. You can also omit the option to build for all targets, or specify a different list.

The next to last — `--firmwarepath` — tells qemu where to look for firmware. The default is `share/qemu`, relative to `--prefix`. Without it, QEMU can't find things like `vgabios-stdvga.bin`. On my system, I need to add `/usr/share/qemu`, for the `OVMF.fd` file. This may need tweaking depending on the system. Furthermore, for this to work when QEMU is launched from Multipass, the apparmor profile needs to cover these locations, as in https://github.com/canonical/multipass/pull/3152/files.

For the remaining options, again look at the yaml for what we're currently using.

*Bonus*: Add `--enable-debug` if interested in debugging QEMU.

References:
  - `autotools-configure-parameters` for qemu in our `snapcraft.yaml`
  - `configure --help`

## 4 - Build and Install

To build, simply run

```sh
ninja
```

Next, we need to install our locally built binaries in front of QEMU binaries installed via `apt`. To find out where things will be installed by default, do:

```sh
$ ../configure --help | grep '\-\-prefix='
```

In my case the default prefix is `/usr/local` which is what I need since `/usr/local/bin` is in front of `/usr/bin` in my `PATH`. If you need a different prefix, you can use `--prefix` in the configuration step, but you will probably need to modify the apparmor profile for QEMU in Multipass.

To install, do:

```sh
sudo ninja install
```

(The converse `sudo ninja uninstall` uninstalls.)

## 5 - Verify

Check that you get the expected QEMU with:

```
hash -r
which qemu-system-x86_64
qemu-system-x86_64 --version
```

Finally launch a Multipass instance and try a native mount.
