# cloud-init-dhcp-hostname

A NoCloud cloud-init seed ISO used by the Hyper-V HCN DNS end-to-end integration
test (`HyperV_ComponentIntegrationTests.dns_suffix_hostname_resolution` in
`tests/unit/hyperv_api/test_bb_cit_hyperv.cpp`).

It is identical in spirit to `../cloud-init/`, but its `user-data` adds a
`runcmd` that re-runs `udhcpc` with `-x hostname:<hostname>` once the network is
up. Stock Alpine's busybox `udhcpc` does not send DHCP option 12 (host name) by
default, so without this the ICS DHCP/DNS proxy never learns the guest's name
and `<hostname>.<domain>` cannot be resolved from the host.

- `meta-data`  — sets `local-hostname: multipass-alpine-it`.
- `user-data`  — sets passwords and forces the hostname-advertising DHCP renew.
- `cloud-init.iso` — the built NoCloud seed (volume label `cidata`).

## Regenerating `cloud-init.iso`

The ISO is a committed binary fixture (like `../cloud-init/cloud-init.iso`). To
rebuild it from `user-data`/`meta-data`, use any NoCloud-capable ISO authoring
tool, e.g. with Python + pycdlib:

```python
import io, pycdlib
iso = pycdlib.PyCdlib()
iso.new(interchange_level=3, joliet=3, rock_ridge="1.09", vol_ident="cidata")
for name in ("user-data", "meta-data"):
    data = open(name, "rb").read()
    iso.add_fp(io.BytesIO(data), len(data),
               "/" + name.upper().replace("-", "_") + ".;1",
               rr_name=name, joliet_path="/" + name)
iso.write("cloud-init.iso")
iso.close()
```

Or on Linux: `genisoimage -output cloud-init.iso -volid cidata -joliet -rock user-data meta-data`.
