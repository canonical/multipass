# Build instructions for Linux

## Environment Setup

Note: if building on arm, s390x, ppc64le, or riscv, environment variable `VCPKG_FORCE_SYSTEM_BINARIES` must be set:

```
export VCPKG_FORCE_SYSTEM_BINARIES=1
```

### Build dependencies

```
cd <multipass>
sudo apt install devscripts equivs
mk-build-deps -s sudo -i
```

## Building

```
cd <multipass>
git submodule update --init --recursive
mkdir build
cd build
cmake ../
cmake --build . --parallel
```

Please note that if you're working on a forked repository that you created using the "Copy the main branch only" option, the repository will not include the necessary git tags to determine the Multipass version during CMake configuration. In this case, you need to manually fetch the tags from the upstream by running `git fetch --tags https://github.com/canonical/multipass.git` in the `<multipass>` source code directory.

### Automatic linker selection

***Requires (>= CMake 3.29)***

To accelerate the build, the build system will attempt to locate and utilize `mold` or `lld` (respectively) in place of the default linker of the toolchain. To override, set [CMAKE_LINKER_TYPE](https://cmake.org/cmake/help/latest/variable/CMAKE_LINKER_TYPE.html#cmake-linker-type) at CMake configure step.

## Run the Multipass daemon and client

First, install Multipass's runtime dependencies. On AMD64 architecture, you can do this with:

```
sudo apt update
sudo apt install libgl1 libpng16-16 libqt6core6 libqt6gui6 \
    libqt6network6 libqt6widgets6 libxml2 libvirt0 dnsmasq-base \
    dnsmasq-utils qemu-system-x86 qemu-utils libslang2 iproute2 \
    iptables iputils-ping libatm1 libxtables12 xterm
```

Then run the Multipass daemon:

```
sudo <multipass>/build/bin/multipassd &
```

Copy the desktop file that Multipass clients expect to find in your home:

```
mkdir -p ~/.local/share/multipass/
cp <multipass>/data/multipass.gui.autostart.desktop ~/.local/share/multipass/
```

Optionally, enable auto-complete in Bash:

```
source <multipass>/completions/bash/multipass
```

Now you can use the `multipass` command from your terminal (for example `<multipass>/build/bin/multipass launch --name foo`) or launch the GUI client with the command `<multipass>/build/bin/multipass.gui`.
