# vcpkg overlays

This folder contains the custom vcpkg triplet overlays, port overlays, and port hooks.

## Rules for triplets

* For every single vcpkg triplet the project is using, there *MUST* be a triplet overlay.
* All triplet overlay files *MUST* include `multipass-triplet-common.cmake`. This module enables pre/post portfile hooks.

## Hooks

The hooks/ folder contains the hook CMake files for a `vcpkg` package. `hooks` are less intrusive way of altering `portfile.cmake` behavior.

To implemeht hooks for a vcpkg package, simply create a subfolder with the package's name under `hooks/` directory. Right now, there are two possible hook points, namely `pre_portfile.cmake` and `post_portfile.cmake` . The names are self-explanatory -- meaning the `pre/post` hook files would be included just before/after the target package's `portfile.cmake` , offering a customization point without having to overhaul the whole port. The hook files would have unrestricted access to the target `portfile.cmake` 's scope, making it possible to alter behavior programmatically.

### Use-case

The primary use-case of hook files are to eliminate unnecessary targets from vcpkg dependencies to cut down the build time.

### MP_VCPKG_DISABLE_CMAKE_TARGETS

`MP_VCPKG_DISABLE_CMAKE_TARGETS` is a special portfile hook variable that allows disabling CMake targets altogether for a portfile. It allows trimming down dependencies programmatically.

To demonstrate it, let's assume the following:

* Project "bar" depends on package "foo"
* Package "foo" defines "a_foo", "b_foo" and "c_foo" targets
* Project "bar" only needs the "a_foo", making building "b_foo" and "c_foo" redundant.
* Package "foo"'s portfile does not offer any options to disable "b_foo or "c_foo"

The `vcpkg` way of dealing with this is to copy the vcpkg port for "foo" as an overlay, make a patch file that edits the CMakeLists.txt file for "foo" and then maintain that port overlay.

The alternative way is to define a `pre_portfile.cmake` hook and use `MP_VCPKG_DISABLE_CMAKE_TARGETS` to disable "b_foo" and "c_foo" programmatically:

* Create folder "foo" under `vcpkg-overlays/hooks`
* Create a file called `pre_portfile.cmake` under `vcpkg-overlays/hooks/foo` with the following content:

```cmake
set(MP_VCPKG_DISABLE_CMAKE_TARGETS b_foo c_foo)
```

That's it. For a concrete example, see `vcpkg-overlays/hooks/grpc` .
