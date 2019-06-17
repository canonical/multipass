# What is multipass?

It's a system that orchestrates the creation, management and maintenance
of virtual machines and associated Ubuntu images to simplify development.

# Getting it

On Linux it's available as a classically confined snap, in the `beta` channel:

```
sudo snap install multipass --beta --classic
```

For macOS, you can download the installers [from GitHub](https://github.com/CanonicalLtd/multipass/releases) or [use Homebrew](https://github.com/Homebrew/brew):

```
brew cask install multipass
```

# Usage

## Launch an instance
```
multipass launch --name foo
```

## Run commands in an instance
```
multipass exec foo -- lsb_release -a
```

## Pass a cloud-init file to an instance
```
multipass launch -n foo --cloud-init cloud-config.yaml
```

See [Using cloud-init with Multipass](https://blog.ubuntu.com/2018/04/02/using-cloud-init-with-multipass) on the Ubuntu blog for more details.

## Find available images
```
multipass find
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

## Connect to a running instance

```
<multipass>/build/bin/multipass shell foo
```
