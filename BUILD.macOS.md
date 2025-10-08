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

### Cmake/OpenSSL

Building a Multipass package requires cmake 3.9 or greater. OpenSSL is also necessary at build time. The most convenient
means to obtain these dependencies is with Homebrew <https://brew.sh/>.

    brew install cmake openssl@3 glib pixman

Building
---------------------------------------

### Additional configuration

If you encounter errors about missing `pkg-config` or `ninja`, install them with:

    brew install pkg-config ninja

This is required for CMake to find all necessary build tools and dependencies.

If you encounter Python errors about missing `tomli` during the QEMU build step, install it with:

    pip3 install tomli

This is required for Python-based build scripts.

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

Then start the build with:

    cd build/
    ninja

Take care to adjust the `CMAKE_PREFIX_PATH` to the location you installed Qt above, or else cmake will complain about
missing Qt6.

Building in QtCreator
---------------------
QtCreator will be missing all the environment adjustments made above. To get cmake to successfully configure, open the
project and adjust the Build Environment (click the "Projects" icon of the left pane, scroll down). Then add the entries
to the $PATH as above, and add the variables reported by `opem config env`. CMake should now succeed, and QtCreator
allow you to edit the project files.


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
