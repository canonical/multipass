# vcpkg overlay ports

This directory holds our own vcpkg ports: complete ports with no upstream vcpkg counterpart (e.g.
`qemu`). vcpkg uses them directly, as overlays.

Our changes to upstream ports are in [`../vcpkg-port-patches`](../vcpkg-port-patches/README.md).

## Updating

Own ports are entirely ours to update: version, source hash, and patches.

## Adding a port

Add the complete port here. Nothing else is needed.
