# cloud-init-fqdn-multipass

Builds the NoCloud cloud-init seeds used by the Hyper-V HCN DNS end-to-end
integration tests (the `HyperV_ComponentIntegrationTests.dns_suffix_*` tests in
`tests/unit/hcs/test_bb_cit_hyperv.cpp`).

Unlike the other fixtures, the ISO here is **generated at run time** by
`make_seed_iso.py` rather than committed as a binary. Each test passes a *unique*
DHCP hostname per run (e.g. `mp-host-1dd6e733`, or the dotted
`mp-fqdn-1dd6e733.multipass`). This matters because the ICS DNS proxy persists
its name→IP registrations across runs (for about a week) and cannot be flushed
without restarting SharedAccess (which HNS pins) — so reusing a fixed hostname
could resolve to a stale lease from an earlier run, and hide a broken
registration.

Stock Alpine's busybox `udhcpc` does not send DHCP option 12 (host name), so the
ICS DHCP/DNS proxy would never learn the guest's name. The seed re-runs `udhcpc`
with `-x hostname:<hostname>` once the network is up, and the proxy registers the
guest as `<hostname>.<ICSDomain>` (`mshome.net` by default).

## Custom suffixes without global state mutation

`dns_suffix_fqdn_append_resolution` and the multi-zone tests rely on the hostname
being **dotted**:

1. The guest advertises a **dotted** DHCP host name `<label>.multipass`
   (DHCP option 12, via `udhcpc -x hostname:<label>.multipass`). The ICS DNS
   proxy stores it as `<label>.multipass.mshome.net`, appending the **global**
   `ICSDomain` (`mshome.net`) verbatim — without any change to that global value.
2. The host appends the `mshome.net` search suffix to multi-label names
   (`AppendToMultiLabelName=1` + `SuffixSearchList=mshome.net`), so a typed
   `<label>.multipass` that fails as-is is retried as
   `<label>.multipass.mshome.net`.
3. A narrow NRPT rule (`.multipass.mshome.net` → gateway) routes the appended
   query to the ICS proxy, which answers with the guest's live leased IP.

Net result: `<label>.multipass` resolves via the system resolver to the live
lease, while the global `ICSDomain` stays `mshome.net` the whole time. No
time-windowed registry values, no extra DNS server/resolver process.

## Files

- `make_seed_iso.py` — builds the seed ISO for a given `--hostname`/`--output`.
  The `user-data`/`meta-data` templates are embedded in the script.

## Requirements

`python` (3.x) on the `PATH`, with [`pycdlib`](https://pypi.org/project/pycdlib/)
installed (`python -m pip install pycdlib`). Without it, the tests fail with
"seed ISO build failed".

## Regenerating manually

```
python make_seed_iso.py --hostname mp-fqdn-deadbeef.multipass --output cloud-init.iso
```
