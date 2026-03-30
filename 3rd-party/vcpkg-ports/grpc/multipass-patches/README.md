# Multipass vcpkg gRPC patch

This patch mechanism trims down the gRPC port in favor of better build times.

## What is removed?

The following parts of the gRPC are removed by this patch:

- The unsecure gRPC libs (grpc_unsecure grpc++_unsecure)
- Authorization provider (grpc_authorization_provider)
- Reflection (grpc++_reflection)
- grpc++_alts
- Language plugins (PHP, Python, Node, Objective C, CSharp, Ruby)

## Removal method

The language plugins have corresponding `gRPC_BUILD_GRPC_<lang-name>_PLUGIN` variables. They are passed via `VCPKG_CMAKE_CONFIGURE_OPTIONS` so it's a little bit less intrusive. Still, the `vcpkg_copy_tools` call needs to be altered since they are no longer being built, and vcpkg does not have a clue about that.

The rest of the parts are dynamically patched out via the trampoline `CMakeLists.txt` file in this folder. The way it's done is twofold. First, we do a deferred call to the `do_target_postprocessing` function, which marks unwanted targets with `EXCLUDE_FROM_ALL` as the last thing in the cmake configure call, which is sufficient to prevent them from being built. Their installation calls, however still needs to be patched out since the `install()` calls won't care if the target is built or not. To do that, the trampoline `CMakeLists.txt` also patches the `install()` call so they can be filtered.

## Upgrade process

The upgrade steps must be followed whenever the gRPC version is needed to be updated.

Step 1: Remove everything from the grpc port folder except the multipass-patches folder
Step 2: Copy the vcpkg's grpc port to vcpkg-ports folder
Step 3: Open the portfile.cmake file in the newly copied port folder, add the following just below the `vcpkg_from_github` call:

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/multipass-patches/grpc-remove-unneeded-libs.cmake)
```

Step 4: Find the vcpkg_copy_tools call, remove the following entries (i.e., everything except grpc_cpp_plugin):

```cmake
grpc_php_plugin
grpc_python_plugin
grpc_node_plugin
grpc_objective_c_plugin
grpc_csharp_plugin
grpc_ruby_plugin
```

Step 5: Profit.
