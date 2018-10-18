# What is multipass?

It's a system that orchestrates the creation, management and maintenance
of virtual machines and associated Ubuntu images to simplify development.

# Getting it

It's available as a classically confined snap, in the `beta` channel:

```
sudo snap install multipass --beta --classic
```

# Usage

## Launch an instance
```
multipass launch --name foo
```

## Run commands in an instance
```
multipass exec foo lsb_release -a
```

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

## Install snapcraft

```
sudo snap install snapcraft --classic
```

## Build and install the snap

```
cd <multipass>
git submodule update --init --recursive
snapcraft
snap install --dangerous --classic multipass*.snap
```

## More info about building snaps

More info about building and developing snaps can be found [here](https://docs.snapcraft.io/t/clean-build-using-lxc/4157).
