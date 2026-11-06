# vcpkg port patches

This directory holds our changes to vcpkg ports that exist upstream, in vcpkg's registry (the
`3rd-party/vcpkg` submodule). Our own custom ports are in
[`../vcpkg-ports`](../vcpkg-ports/README.md).

Each directory here is named after the upstream port that it patches (e.g. `grpc`) and holds
only our changes:

- `portfile.cmake.patch` - our changes to the upstream `portfile.cmake` (required).
- anything the patched portfile refers to, e.g. patches to the port's sources, or CMake scripts. The
  portfile finds them under `multipass-patches/`.

When configuring CMake, our patches are taken together with the upstream port to generate the final
port in the build directory (see `src/cmake/vcpkg-overlay-ports.cmake`), which is then registered as
an overlay (so patched ports are not listed in `vcpkg-configuration.json`).

## Updating

Patched ports follow the vcpkg submodule: update it, along with `builtin-baseline` in `vcpkg.json`,
and reconfigure. If `portfile.cmake.patch` no longer applies, update it as described below. If one
of the other patches no longer applies, vcpkg fails to build the port, so update it for the new
sources. Even if everything applies, consider reviewing the upstream changes to the port
(`git -C 3rd-party/vcpkg diff <old>..<new> -- ports/<port>`) and to its sources, for anything that
calls for new modifications (e.g. new gRPC plugins to disable).

Note that changing files in this directory does not trigger a reconfiguration by itself.

## (Re)creating `portfile.cmake.patch`

Use `git diff --output`, rather than shell redirection, to preserve exact bytes. Producing the diff
from directories named `a` and `b`, with `--no-prefix`, yields the paths that the patch needs:

```sh
port=grpc # for example
patch="$PWD/3rd-party/vcpkg-port-patches/$port/portfile.cmake.patch"
tmp=$(mktemp -d)
for d in a b; do mkdir "$tmp/$d" && cp "3rd-party/vcpkg/ports/$port/portfile.cmake" "$tmp/$d/"; done
(cd "$tmp/b" && git apply --reject "$patch") # when updating; then resolve any portfile.cmake.rej
# edit "$tmp/b/portfile.cmake" as needed
(cd "$tmp" && git diff --no-index --no-prefix --output="$patch" a/portfile.cmake b/portfile.cmake)
```

## Adding a port

Create `<port>/portfile.cmake.patch` here, along with anything it refers to. Nothing else is needed.
