(explanation-service)=
# Service

> See also: [Command-line interface](/reference/command-line-interface/index), [GUI client](/reference/gui-client), [Instance](/explanation/instance), [Use Multipass remotely](/)

In Multipass, the **service** refers to the Multipass server that clients connect to and controls and manages Multipass instances.  This can also be referred to as the 'daemon'. Multipass daemon (`multipassd)` runs in the background and it processes the requests from the Multipass [command-line interface](/reference/command-line-interface/index), [GUI client](/reference/gui-client), daemon is also in charge of the lifecycle of the [Instances](/explanation/instance). The separation between the client (CLI or GUI) and daemon is a popular architecture because of his advantage, flexibility. For instance, the daemon process can be on a different machine from the client, see [Use Multipass remotely](/) for more details.

The automatic start of the daemon process is triggered right after the Multipass installation. After that, it is also set up to start automatically at system boot. This setup ensures that the Multipass client can immediately interact with the instances without the need to launch the daemon manually, and restores any persistent instances of Multipass after a system restart.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/service" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

