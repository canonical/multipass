Build instructions for Mac OS
=============================

Environment Setup
-----------------
### XCode
Can be installed via the App Store. Once this is done, you may need to install the command line tooling too, to do this run:

    xcode-select --install

Ensure you have development Frameworks for at least OS X 10.8 installed, with the typical compiler toolchain and "git". Avoid the version of cmake supplied, we need a newer one (see later).

### Qt5
#### Option 1: Using Qt official sources
Install the latest stable version of Qt5 LTS (5.15.3 at the moment): <http://www.qt.io/download-open-source/>.

If it tells you that XCode 5.0.0 needs to be installed, go to XCode > Preferences > Locations and make a selection in the _Command Line Tools_ box.

Add Qt5 to your PATH environment variable, adding to your `.bash_profile` file the following line:

    export PATH=$PATH:~/Qt/5.15.3/clang_64/bin

Adjust accordingly if you customized the Qt install directory.

#### Option 2: Using Homebrew
Install Qt5:

    brew install qt5

### Cmake/OpenSSL
Building a Multipass package requires cmake 3.9 or greater. OpenSSL is also necessary at build time. The most convenient means to obtain these dependencies is with Homebrew <https://brew.sh/>.

    brew install cmake openssl@1.1

### Hyperkit dependencies (Intel-based Mac only)
Hyperkit is a core utility used by Multipass. To build it we need more dependencies:

    brew install wget opam dylibbundler libffi pkg-config

Answer `yes` when it asks to modify your `.bash_profile`, or otherwise ensure `` eval `opam config env` `` runs in your shell.

Hyperkit's QCow support is implemented in an OCaml module, which we need to fetch using the OPAM packaging system. Initialize:

    export OPAM_COMP=4.11.1
    opam init --comp="${OPAM_COMP}" --switch="${OPAM_COMP}"
    opam update

Install the required modules:

    opam install 3rd-party/hyperkit --deps-only

Building
---------------------------------------
    cd <multipass>
    git submodule update --init --recursive

To build with official Qt sources do:

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=~/Qt/5.12.9/clang_64

Alternatively if using Qt5 from Homebrew, do

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=/usr/local/opt/qt5

or, if on Apple silicon, brew will store the Qt binaries in a different location. Additionally, OpenSSL will be in a similar location; `/opt/homebrew/Cellar/openssl@1.1/1.1.1t`, which can be set in the project level `CMakeLists.txt` file.

    cmake -Bbuild -H. -GNinja -DCMAKE_PREFIX_PATH=/opt/homebrew/Cellar/qt@5/5.15.8_3

Then start the build with:

    cd build/
    ninja

Take care to adjust the `CMAKE_PREFIX_PATH` to the location you installed Qt above, or else cmake will complain about missing Qt5.

Building in QtCreator
---------------------
QtCreator will be missing all the environment adjustments made above. To get cmake to successfully configure, open the project and adjust the Build Environment (click the "Projects" icon of the left pane, scroll down). Then add the entries to the $PATH as above, and add the variables reported by `opem config env`. CMake should now succeed, and QtCreator allow you to edit the project files.


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
