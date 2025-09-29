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

### Qt6

#### Installing Qt 6.2.4 using aqtinstall (recommended)

Install aqtinstall and use it to download Qt 6.2.4:

    pip3 install aqtinstall
    python3 -m aqt install-qt mac desktop 6.2.4 --outputdir ~/Qt

This will install Qt 6.2.4 to `~/Qt/6.2.4/macos`.

Adjust accordingly if you customized the Qt install directory.

### CocoaPods

CocoaPods is required to build the Flutter GUI

    sudo gem install cocoapods

You may need to update your version of Ruby first. You can do so with RVM:

    rvm install 3.1.0 && rvm --default use 3.1.0

**Important: RVM/Ruby may require OpenSSL v1, while building Multipass requires OpenSSL v3. Make sure to switch to the right version of OpenSSL as necessary with `brew link`**


Install Qt6:

    brew install qt6

### Cmake

Building a Multipass package requires cmake 3.9 or greater. The most convenient
means to obtain cmake is with Homebrew <https://brew.sh/>.

    brew install cmake glib pixman

Building
---------------------------------------

### Additional configuration

If you encounter errors about missing `pkg-config` or `ninja`, install them with:

    brew install pkg-config ninja

This is required for CMake to find all necessary build tools and dependencies.

#### QEMU Configuration

QEMU is required to build the Multipass tests:

    brew install qemu

If you encounter Python errors about missing `tomli` during the QEMU build step, install it with:

    pip3 install tomli

This is required for Python-based build scripts.

You may need additional libraries and packages during the configuration process:

    brew install glib pixman

#### Xcode setup

After installing Xcode, you may need to configure the command line tools and complete the initial setup:

    sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
    sudo xcodebuild -runFirstLaunch

The first command sets the active developer directory to your Xcode installation. This is necessary when xcodebuild is not found in the PATH or when there are multiple Xcode installations.

The second command runs Xcode's first launch setup, which installs additional components and tools required for building iOS and macOS applications. This may be needed if you encounter errors about missing Xcode tools or frameworks during the build process.

### Install Submodules

    cd <multipass>
    git submodule update --init --recursive

### Build Multipass

To build with Qt installed via aqtinstall:

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=~/Qt/6.2.4/macos/lib/cmake

Alternatively if using Qt6 from Homebrew, do

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=/usr/local/opt/qt6

or, if on Apple silicon, brew will store the Qt binaries in a different location.

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6

Then start the build with:

    cd build/
    ninja

Take care to adjust the `CMAKE_PREFIX_PATH` to the location you installed Qt above, or else cmake will complain about
missing Qt6.

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
