(reference-command-line-interface-networks)=
# networks

> See also: [Driver (backend)](/explanation/driver), [How to set up the driver](/how-to-guides/customise-multipass/set-up-the-driver)

The `multipass networks` command lists network interfaces that multipass can connect instances to. The result depends both on the platform and the driver in use.

At this time, `multipass networks` can only find interfaces in the following scenarios:

- on Linux, with QEMU
- on Windows, with both Hyper-V and VirtualBox
- on macOS, with the QEMU and VirtualBox drivers

For example, on Windows with Hyper-V the `multipass networks` command returns:

```{code-block} text
Name            Type    Description
Default Switch  switch  Virtual Switch with internal networking
ExternalSwitch  switch  Virtual Switch with external networking via "Red Hat VirtIO Ethernet Adapter"
InternalSwitch  switch  Virtual Switch with internal networking
PrivSwitch      switch  Private virtual switch
```

Like [`list`](/reference/command-line-interface/list), `networks` supports the `--format` option.

Another example, running the command `multipass networks --format yaml` on macOS with VirtualBox returns:

```{code-block} text
bridge0:
  - type: bridge
    description: Network bridge with en1, en2
bridge2:
  - type: bridge
    description: Empty network bridge
en0:
  - type: wifi
    description: Wi-Fi (Wireless)
en1:
  - type: thunderbolt
    description: Thunderbolt 1
en2:
  - type: thunderbolt
    description: Thunderbolt 2
```

See [`launch`](/reference/command-line-interface/launch) and {ref}`create-an-instance-with-multiple-network-interfaces` for instructions on how to use these.

---

The `multipass help networks` command explains the available options:

```{code-block} text
Usage: multipass networks [options]
List host network devices (physical interfaces, virtual switches, bridges)
available to integrate with using the `--network` switch to the `launch`
command.

Options:
  -h, --help         Displays help on commandline options
  -v, --verbose      Increase logging verbosity. Repeat the 'v' in the short
                     option for more detail. Maximum verbosity is obtained with
                     4 (or more) v's, i.e. -vvvv.
  --format <format>  Output list in the requested format.
                     Valid formats are: table (default), json, csv and yaml
```
