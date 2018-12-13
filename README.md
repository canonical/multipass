# What is multipass?

Multipass is a lightweight VM manager for Linux and MacOS. It's designed for
developers who want a fresh Ubuntu with a single command or hotkey. It uses
KVM on Linux and HyperKit on MacOS to run the VM with minimal overhead.
Multipass will fetch images for you and keep them up to date.

Since it supports metadata for cloud-init, you can simulate a small cloud
deployment on your laptop or workstation.

# Install Multipass

On Linux it's available as a snap:

```
sudo snap install multipass --beta --classic
```

For macOS, you can download the installers [from GitHub](https://github.com/CanonicalLtd/multipass/releases) or [use Homebrew](https://github.com/Homebrew/brew):

```
brew cask install multipass
```

# Usage

## Find available images
```
$ multipass find
IMAGE  ALIASES         VERSION   DESCRIPTION
core                   20180419  Ubuntu Core 16
14.04  t, trusty       20181203  Ubuntu 14.04 LTS
16.04  x, xenial       20181207  Ubuntu 16.04 LTS
18.04  b, bionic, lts  20181207  Ubuntu 16.04 LTS
```

## Launch a fresh instance of the current Ubuntu LTS
```
$ multipass launch ubuntu
Launching dancing-chipmunk...
Downloading Ubuntu 18.04 LTS..........
Launched: dancing chipmunk
```

## Check out the running instances
```
$ multipass list
Name                    State             IPv4             Release
dancing-chipmunk        RUNNING           10.125.174.247   Ubuntu 18.04 LTS
live-naiad              RUNNING           10.125.174.243   Ubuntu 18.04 LTS
snapcraft-asciinema     STOPPED           --               Ubuntu Snapcraft builder for Core 18
```

## Connect to a running instance

```
$ multipass shell dancing-chipmunk
Welcome to Ubuntu 18.04.1 LTS (GNU/Linux 4.15.0-42-generic x86_64)

...
```

Don't forget to logout (or Ctrl-D) or you may find yourself heading all the
way down the Inception levels... ;)

## Run commands in an instance from outside
```
$ multipass exec dancing-chipmunk -- lsb_release -a
No LSB modules are available.
Distributor ID:  Ubuntu
Description:     Ubuntu 18.04.1 LTS
Release:         18.04
Codename:        bionic
```

## Get help
```
multipass help
multipass help <command>
```

# Get involved!

Here's a set of steps to build and run your own build of multipass:

## Build Dependencies

```
cd <multipass>
apt install devscripts equivs
mk-build-deps -s sudo -i
```

## Building

```
cd <multipass>
git submodule update --init --recursive
mkdir build
cd build
cmake ../
make
```

## Running multipass daemon and client

```
sudo <multipass>/build/bin/multipassd &
<multipass>/build/bin/multipass launch --name foo
```

