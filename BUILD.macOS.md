Build instructions for Mac OS
=============================

Environment Setup
-----------------

### XCode

Can be installed via the App Store. Once this is done, you may need to install the command line tooling too, to do this
run:

    xcode-select --install

Ensure you have development Frameworks for at least OS X 10.8 installed, with the typical compiler toolchain and "git".
Avoid the version of cmake supplied, we need a newer one (see later).

### Homebrew

Install Homebrew <https://brew.sh/> for package management. It is the most straightforward way of installing the build's dependencies.

### CocoaPods

Install Qt6:

    brew install qt6

### Cmake

Building a Multipass package requires cmake 3.9 or greater. The most convenient
means to obtain cmake is with Homebrew <https://brew.sh/>.

    brew install cmake

Building
---------------------------------------

    cd <multipass>
    git submodule update --init --recursive

To build with official Qt sources do:

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=~/Qt/6.10.0/clang_64

Alternatively if using Qt6 from Homebrew, do

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=/usr/local/opt/qt6

or, if on Apple silicon, brew will store the Qt binaries in a different location.

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6

Then start the build with:

    cd build/
    ninja

Creating a Package
------------------
This is as simple as running

    ninja package

or

    cpack

Once it is complete, you will have a Multipass.pkg file in the build directory.


Tips and Tricks for development on OSX
======================================

Getting console output
----------------------
To get console output from Multipass while installed, from one shell run:

    sudo launchctl debug system/com.canonical.multipassd --stdout --stderr

and then from another shell, restart the multipassd daemon with

    sudo launchctl kickstart -k system/com.canonical.multipassd

The console output from multipassd will appear on the former shell.
