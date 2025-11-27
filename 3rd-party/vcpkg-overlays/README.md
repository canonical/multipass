# vcpkg overlays

This folder contains the custom vcpkg triplet overlays, port overlays, and port target filters.

## Rules for triplets

* For every single vcpkg triplet the project is using, there *MUST* be a triplet overlay.
* All triplet overlay files *MUST* include `multipass-triplet-common.cmake`. This module enables the target filtering code.

## Target filters

The primary use-case of target filters are to eliminate unnecessary targets from vcpkg dependencies to cut down the build time.

### MP_VCPKG_DISABLE_CMAKE_TARGETS

`MP_VCPKG_DISABLE_CMAKE_TARGETS` is a special variable that allows disabling CMake targets altogether for a portfile. It allows trimming down dependencies programmatically.

To demonstrate it, let's assume the following:

* Project "bar" depends on package "foo"
* Package "foo" defines "a_foo", "b_foo" and "c_foo" targets
* Project "bar" only needs the "a_foo", making building "b_foo" and "c_foo" redundant.
* Package "foo"'s portfile does not offer any options to disable "b_foo or "c_foo"

The `vcpkg` way of dealing with this is to copy the vcpkg port for "foo" as an overlay, make a patch file that edits the CMakeLists.txt file for "foo" and then maintain that port overlay.

The alternative way is to define a `include.cmake` file and use `MP_VCPKG_DISABLE_CMAKE_TARGETS` to disable "b_foo" and "c_foo" programmatically:

* Create folder "foo" under `vcpkg-overlays/target-filters`
* Create a file called `include.cmake` under `vcpkg-overlays/target-filters/foo` with the following content:

```cmake
set(MP_VCPKG_DISABLE_CMAKE_TARGETS b_foo c_foo)
```

That's it. For a concrete example, see `vcpkg-overlays/target-filters/grpc` .
