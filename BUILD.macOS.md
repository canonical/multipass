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

### Qt6

#### Installing Qt 6.10.0 using aqtinstall (recommended)

Install aqtinstall and use it to download Qt 6.10.0:

    brew install aqtinstall
    aqt install-qt mac desktop 6.10.0 --outputdir ~/Qt

This will install Qt 6.10.0 to `~/Qt/6.10.0/macos`.

Adjust accordingly if you customized the Qt install directory.

### CocoaPods

CocoaPods is required to build the Flutter GUI

    sudo gem install cocoapods

You may need to update your version of Ruby first. You can do so with RVM <https://rvm.io/rvm/install>:

    rvm install 3.1.0 && rvm --default use 3.1.0

**Important: RVM/Ruby may require OpenSSL v1, while building Multipass requires OpenSSL v3. Make sure to switch to the right version of OpenSSL as necessary with `brew link`**

**Additional note**: `gem install` may not work with `sudo`, run the command without it if that is the case.


### Cmake

Building a Multipass package requires cmake 3.9 or greater. The most convenient
means to obtain cmake is with Homebrew <https://brew.sh/>.

    brew install cmake

Building
---------------------------------------

### Additional configuration

If you encounter errors about missing `pkg-config` or `ninja`, install them with:

    brew install pkg-config ninja

This is required for CMake to find all necessary build tools and dependencies.

#### Dylibbundler

Dylibbundler is required to make Multipass packages:

    brew install dylibbundler

#### QEMU Configuration

QEMU is required to build the Multipass tests:

    brew install qemu

If you encounter Python errors about missing `tomli` during the QEMU build step, install it with:

    pip3 install tomli

This is required for Python-based build scripts.

You may need additional libraries and packages during the configuration process:

    brew install glib pixman

If the build process lacks `distlib` when creating virtual environments, you will need to either install it using:

**Option 1**:

```
python3 -m venv ~/multipass-build-env
source ~/multipass-build-env/bin/activate
pip install distlib
```
And then install all packages that are required during the build. Afterwards, add to the cmake configure command the flag `-DPYTHON_EXECUTABLE=$VIRTUAL_ENV/bin/python3`.

**Option 2**:

Since `distlib` is a build-time dependency, alternatively you can install the package globally.
```
/usr/local/bin/python3 -m pip install distlib --break-system-packages
```
#### Xcode setup

After installing Xcode, you may need to configure the command line tools and complete the initial setup:

    sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
    sudo xcodebuild -runFirstLaunch

The first command sets the active developer directory to your Xcode installation. This is necessary when xcodebuild is not found in the PATH or when there are multiple Xcode installations.

The second command runs Xcode's first launch setup, which installs additional components and tools required for building iOS and macOS applications. This may be needed if you encounter errors about missing Xcode tools or frameworks during the build process.

### Install Submodules

    cd <multipass>
    git submodule update --init --recursive
    mkdir build
    cd build

CMake will fetch all necessary content, build vcpkg dependencies, and initialize the build system.
You can also specify the `-DCMAKE_BUILD_TYPE` option to set the build type (e.g., `Debug`,
`Release`, `Coverage`, etc.).

### Build Multipass

    cmake -GNinja -DCMAKE_PREFIX_PATH=~/Qt/6.10.0/macos

Alternatively if using Qt6 from Homebrew, do

    cmake -GNinja -DCMAKE_PREFIX_PATH=/usr/local/opt/qt6

or, if on Apple silicon, brew will store the Qt binaries in a different location. Additionally, OpenSSL will be in a
similar location; `/opt/homebrew/Cellar/openssl@3`, which can be set in the project level `CMakeLists.txt` file.

    cmake -GNinja -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt6

Finally, to build the project, run:

    cmake --build . --parallel

Take care to adjust the `CMAKE_PREFIX_PATH` to the location you installed Qt above, or else cmake will complain about
missing Qt6.

Creating a Package
------------------
This is as simple as running

    cmake --build . --target package

Make sure you have dylibbundler installed!

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
