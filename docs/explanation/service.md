(explanation-service)=
# Service

> See also: [Command-line interface](/reference/command-line-interface/index), [GUI client](/reference/gui-client), [Instance](/explanation/instance)

In Multipass, the **service** refers to the Multipass server that clients connect to and controls and manages Multipass instances. This can also be referred to as the 'daemon'. Multipass daemon (`multipassd)` runs in the background and it processes the requests from the Multipass [command-line interface](/reference/command-line-interface/index), [GUI client](/reference/gui-client), daemon is also in charge of the lifecycle of the [Instances](/explanation/instance). The separation between the client (CLI or GUI) and daemon is a popular architecture because of his advantage, flexibility. For instance, the daemon process can be on a different machine from the client, see [Use Multipass remotely](https://discourse.ubuntu.com/t/how-to-use-multipass-remotely/26360) for more details.

The automatic start of the daemon process is triggered right after the Multipass installation. After that, it is also set up to start automatically at system boot. This setup ensures that the Multipass client can immediately interact with the instances without the need to launch the daemon manually, and restores any persistent instances of Multipass after a system restart.
