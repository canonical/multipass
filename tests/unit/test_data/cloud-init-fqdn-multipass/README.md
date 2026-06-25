# cloud-init-fqdn-multipass

A NoCloud cloud-init seed used by the Hyper-V HCN custom-DNS-suffix end-to-end
integration test
(`HyperV_ComponentIntegrationTests.dns_suffix_fqdn_append_resolution` in
`tests/unit/hyperv_api/test_bb_cit_hyperv.cpp`).

Unlike the other fixtures, the ISO here is **generated at run time** by
`make_seed_iso.py` rather than committed as a binary. The test passes a *unique*
dotted DHCP hostname per run (e.g. `mp-fqdn-1dd6e733.multipass`). This matters
because the ICS DNS proxy persists its name→IP registrations across runs and
cannot be flushed without restarting SharedAccess (which HNS pins) — so reusing a
fixed hostname would resolve to a stale lease from an earlier run.

## The approach under test (no global state mutation)

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

- `make_seed_iso.py` — builds the seed ISO for a given `--hostname`/`--output`
  (requires Python + `pycdlib`). The `user-data`/`meta-data` templates are
  embedded in the script.

## Regenerating manually

```
python make_seed_iso.py --hostname mp-fqdn-deadbeef.multipass --output cloud-init.iso
```
