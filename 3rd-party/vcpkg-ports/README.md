# vcpkg overlay ports

This directory holds the vcpkg ports that override, or add to, those in vcpkg's registry (the
`3rd-party/vcpkg` submodule). There are two kinds.

## Patched upstream ports

Ports that exist upstream, but that we need to change (e.g. `grpc` and `libssh`). Their
directories hold only our changes, under `multipass-patches/`:

- `portfile.cmake.patch` - our changes to the upstream `portfile.cmake`. Its presence is what marks
  a port as patched.
- anything the patched portfile refers to, e.g. patches to the port's sources, or CMake scripts.

When configuring CMake, our patches are taken together with the upstream port to generate the final
port in the build directory (see `src/cmake/vcpkg-overlay-ports.cmake`), which is then registered as
an overlay (so patched ports are not listed in `vcpkg-configuration.json`).

## Own ports

Complete ports with no upstream vcpkg counterpart (e.g. `out-ptr`, `premock`, `qemu`, and
`xz-embedded`). They are listed in `vcpkg-configuration.json` and used directly.

## Updating

- Patched upstream ports follow the vcpkg submodule: update it, along with `builtin-baseline` in
  `vcpkg.json`, and reconfigure. If `portfile.cmake.patch` no longer applies, update it as described
  below. If one of the other patches no longer applies, vcpkg fails to build the port, so update it
  for the new sources. Even if everything applies, consider reviewing the upstream changes to the
  port (`git -C 3rd-party/vcpkg diff <old>..<new> -- ports/<port>`) and to its sources, for
  anything that calls for new modifications (e.g. new gRPC plugins to disable).
- Own ports are entirely ours to update: version, source hash, and patches.

Note that changing files in this directory does not trigger a reconfiguration by itself.

## (Re)creating `portfile.cmake.patch`

Use `git diff --output`, rather than shell redirection, to preserve exact bytes. Producing the diff
from directories named `a` and `b`, with `--no-prefix`, yields the paths that the patch needs:

```sh
port=grpc # for example
patch="$PWD/3rd-party/vcpkg-ports/$port/multipass-patches/portfile.cmake.patch"
tmp=$(mktemp -d)
for d in a b; do mkdir "$tmp/$d" && cp "3rd-party/vcpkg/ports/$port/portfile.cmake" "$tmp/$d/"; done
(cd "$tmp/b" && git apply --reject "$patch") # when updating; then resolve any portfile.cmake.rej
# edit "$tmp/b/portfile.cmake" as needed
(cd "$tmp" && git diff --no-index --no-prefix --output="$patch" a/portfile.cmake b/portfile.cmake)
```

## Adding a port

- To patch an upstream port, create `<port>/multipass-patches/portfile.cmake.patch` here, along with
  anything it refers to. Nothing else is needed.
- To add an own port, add the complete port here and list it in `vcpkg-configuration.json`.
