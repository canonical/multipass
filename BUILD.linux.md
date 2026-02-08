# Build instructions for Linux

## Environment Setup

### Build dependencies

```
cd <multipass>
sudo apt install devscripts equivs
mk-build-deps -s sudo -i
```

## Building

First, go into the repository root and get all the submodules:

```
cd <multipass>
git submodule update --init --recursive
```

If building on arm, s390x, ppc64le, or riscv, you will need to set the `VCPKG_FORCE_SYSTEM_BINARIES`  environment
variable:

```
export VCPKG_FORCE_SYSTEM_BINARIES=1
```

Then create a build directory and run CMake.

```
mkdir build
cd build
cmake ../
```

This will fetch all necessary content, build vcpkg dependencies, and initialize the build system. You can also specify
the `-DCMAKE_BUILD_TYPE` option to set the build type (e.g., `Debug`, `Release`, `Coverage`, etc.).

To use a different vcpkg, pass `-DMULTIPASS_VCPKG_LOCATION="path/to/vcpkg"` to CMake.
It should point to the root vcpkg location, where the top bootstrap scripts are located.

Finally, to build the project, run:

```
cmake --build . --parallel
```

Please note that if you're working on a forked repository that you created using the "Copy the main branch only" option,
the repository will not include the necessary git tags to determine the Multipass version during CMake configuration. In
this case, you need to manually fetch the tags from the upstream by running
`git fetch --tags https://github.com/canonical/multipass.git` in the `<multipass>` source code directory.

## Run the Multipass daemon and client

First, install Multipass's runtime dependencies. On AMD64 architecture, you can do this with:

```
sudo apt update
sudo apt install libgl1 libpng16-16 libxml2 dnsmasq-base \
    dnsmasq-utils qemu-system-x86 qemu-utils libslang2 iproute2 \
    iptables iputils-ping libatm1 libxtables12 xterm
```
On ARM64 architecture, you can do this by running:

```
sudo apt update
sudo apt install libgl1 libpng16-16 libxml2 dnsmasq-base \
    dnsmasq-utils qemu-system-arm qemu-efi-aarch64 qemu-utils \
    libslang2 iproute2 iptables iputils-ping libatm1 libxtables12 \
    xterm
```
Additionally, on ARM64 architecture, there is an extra step to set up the `QEMU_EFI.fd` file:
```
sudo cp /usr/share/qemu-efi-aarch64/QEMU_EFI.fd /usr/share/qemu/QEMU_EFI.fd
```

Then run the Multipass daemon:

```
sudo <multipass>/build/bin/multipassd &
```

Copy the desktop file that Multipass clients expect to find in your home:

```
mkdir -p ~/.local/share/multipass/
cp <multipass>/src/client/gui/assets/multipass.gui.autostart.desktop ~/.local/share/multipass/
```

Optionally, enable auto-complete in Bash and/or Zsh:

```
source <multipass>/completions/bash/multipass
```

```
source <multipass>/completions/zsh/multipass
```

To be able to use the binaries without specifying their path:

```
export PATH=<multipass>/build/bin
```

Now you can use the `multipass` command from your terminal (for example
`<multipass>/build/bin/multipass launch --name foo`) or launch the GUI client with the command
`<multipass>/build/bin/multipass.gui`.
