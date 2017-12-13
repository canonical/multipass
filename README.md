# What is multipass?
It's a system that orchestrates the creation, management and maintenance
of virtual machine instances and associated Ubuntu images to simplify
development.

## Getting it
It's available as a classically confined snap, in the `beta` channel:
```
sudo snap install multipass --beta --classic
```

## Usage
#### Launch an instance
```
multipass launch --name foo
```

#### Run commands in an instance
```
multipass exec foo -- lsb_release -a
```

#### Find available images
```
multipass find
```

#### Get help
```
multipass help
multipass help <command>
```

## Get involved!
Here's a set of steps to build and run your own build of multipass:

#### Build Dependencies
```
apt install devscripts equivs
mk-build-deps -s sudo -i
```

#### Building
```
cd <multipass>
git submodule update --init --recursive
mkdir build
cd build
cmake ../
make multipassd ubuntu
```

#### Running multipass daemon and client
```
sudo <multipass>/build/bin/multipassd &
<multipass>/build/bin/multipass launch --name foo
```

#### Connect to a running instance
```
<multipass>/build/bin/multipass connect foo
```
