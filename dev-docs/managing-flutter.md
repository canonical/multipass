# Managing Flutter

## Updating Flutter

Flutter has a new stable release approximately once every quarter. You can check https://github.com/flutter/flutter/blob/stable/CHANGELOG.md to see if there is a new release. Patch releases can come at any time.

Multipass gets its Flutter from the 3rd-party/flutter git submodule. When a new release comes out, you can get it by going inside the submodule and doing:

```
git fetch
git checkout <release-tag> # go to the actual release tag e.g. 3.32.5
flutter precache # download the new flutter binaries
```

### Updating Flutter dependencies

Flutter dependencies are located in the `pubspec.yaml` file.
After a Flutter update, manually go through the dependencies in that file and check for newer versions on pub.dev.
Theoretically, one could use `flutter pub upgrade --major-versions` for this, but it is a good idea to go through their changelogs.

Some of the dependencies used do not come from pub.dev, but come from GitHub. Those are:

#### [dartssh2](https://github.com/canonical/dartssh2.git)

This is a fork of the dartssh2 package from pub.dev which has two patches:

1. Parse the RSA key numbers from the octetstring sequence

This changes the way we parse RSA keys since the ones we use could not be parsed by the library.

2. Use RawSocket instead of Socket for more customization

This changes how data is read from the socket used by ssh by introducing some delay between reads and doing bigger reads manually, as opposed to having the library always automatically read data when available and pushing it to the stream we listen to.
This was necessary because when there would be a lot of data to read from the socket (think the output of the yes command), the data would be automatically read way too much and it would fill the Dart microtask queue with those read events, and that would not give a chance for Dart to process Flutter events (such as ctrl+c to stop yes), since those events happen on the normal event loop.

#### [tray_menu](https://github.com/canonical/tray_menu.git)

This is the library that Andrei developed to create a system tray icon. Unfortunately it lacks documentation, tests and anything else besides just the needed code, which could also greatly benefit from a refactoring.

#### [xterm](https://github.com/levkropp/xterm.dart)

This is a fork of the xterm package from pub.dev and it has one patch by Lev:

1. Implement line-height snapping and full line rendering which makes it so that the terminal only displays fully visible lines and it scrolls in increments of one line, instead of having continuous scrolling

#### [hotkey_manager](https://github.com/canonical/hotkey_manager.git)

This is a fork of the hotkey_manager package from pub.dev and it has one patch:

1. Disable cooked accelerators which makes the keybinder recognize bindings such as ctrl+shift+1 instead of ctrl+!; see [this](https://github.com/kupferlauncher/keybinder/blob/04ae06724d914c7d4fec6a2723edf9c6320ec502/libkeybinder/bind.c#L535) for details.

#### [window_size](https://github.com/google/flutter-desktop-embedding.git)

There is also the window_size plugin that comes from GitHub, but we do not have any patches for it, it is just not available on pub.dev, so you don’t have to worry about it too much.

The forks mentioned above must also be updated if needed. Check if the upstream package on pub.dev has an update.
If it does, just apply our patches on top of the latest stable version of the package.
Apply the patches on a branch with the name of the version you're upgrading to + the suffix ‘mp’ e.g. when dartssh2 2.12.0 came out, the patches were applied on the 2.12.0+mp branch.
The new branch name must be specified in the `pubspec.yaml` file.

After updating the `pubspec.yaml` packages, do a `flutter pub upgrade` to get the updated `pubspec.lock` file.

If there are any deprecation warnings from `flutter analyze`, fix them sooner rather than later.
Now you must check that the GUI builds properly on all platforms, using `flutter build <macos/linux/windows>`.

When building, Flutter might attempt to modify files in the platform specific macos, linux or windows directories. If it can’t do that because of our existing changes there, it should tell you what it was trying to apply so you can do it manually.

If everything looks fine, commit `pubspec.{yaml,lock}`, as well as any changes in the macos, linux or windows directories.

Lastly, do some linting with `dart format src/client/gui` to make sure that the code is up to date with linting rules.

Now, if everything builds fine in CI as well, you’re done.

### Updating protobuf

This dependency is special because we have a 3rd-party repo and the pubspec.yaml version that we have to keep synchronized. Make sure to update both.

## Multipass GUI

### C++/Flutter glue

One component of the GUI is the dart_ffi library that we build.
This is a C library that wraps some of our C++ code that is also useful inside the GUI.
Keep in mind that this does not allow you to retrieve any information from the daemon, it’s purely a client-side library.

### Failing snapcraft

When trying to build the snap locally, the process might fail.
One possible cause is that the GUI’s source directory is pulled entirely inside the snapcraft VM, and the source directory might contain files that are reference paths on your local machine, which are not valid inside the VM.
This is caused by the fact that we cannot fully separate Flutter’s build files from its source code, and because snapcraft does not have any kind of ignore file where you could tell it not to pull those files.
In order to fix this, delete the snapcraft VM (yes, snapcraft will have to build everything from scratch again :/ ), delete the build, .dart_tool and linux/flutter/ephemeral directories, then try to snap again.
