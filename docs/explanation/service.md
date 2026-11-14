(explanation-service)=
# Service

> See also: [Command-line interface](/reference/command-line-interface/index), [GUI client](/reference/gui-client), [Instance](/explanation/instance)

In Multipass, **service** refers to the Multipass server that clients connect to and that controls and manages Multipass instances. This can also be referred to as the **daemon**.

The Multipass daemon (`multipassd`) runs in the background and processes the requests from the Multipass [command-line interface](/reference/command-line-interface/index) and [GUI client](/reference/gui-client). The daemon is also in charge of the lifecycle of Multipass [instances](/explanation/instance).

The separation between client (CLI or GUI) and daemon is a popular architecture because of its main advantage, that is, flexibility. For instance, the daemon process can be on a different machine from the client; see [Use Multipass remotely](https://discourse.ubuntu.com/t/how-to-use-multipass-remotely/26360) for more details.

The automatic start of the daemon process is triggered right after the Multipass installation. After that, it is also set up to start automatically at system boot. This setup ensures that the Multipass client can immediately interact with the instances without the need to launch the daemon manually, and restores any persistent instances of Multipass after a system restart.

## Snap refreshes

On Linux, Multipass is delivered as a snap and bundles its own QEMU runtime. A snap refresh can therefore replace QEMU under running or suspended instances, and resuming an instance's memory snapshot across QEMU builds is not guaranteed to work: the guest may fail to come back, which is equivalent to a power loss for the guest.

To prevent that, the snap ships a `pre-refresh` hook. snapd runs it (from the currently installed revision, with the services still up) before switching revisions, and a hook failure aborts the refresh. The hook asks the daemon to either *veto* or *prepare* the refresh:

- **Instances with suspended guest memory** (explicitly suspended instances, as well as instances a previous daemon shutdown auto-suspended) block the refresh. Nothing is woken up or discarded automatically: resume the instance and shut it down cleanly (`multipass start <name>`, then `multipass stop <name>`), then retry the refresh. If an instance cannot be resumed, its stale suspended state can be discarded explicitly with `multipass stop --force <name>`.

- **Running instances** are shut down gracefully with the *old* QEMU runtime, and the daemon records which instances were running. If the refresh proceeds, the new daemon starts those instances again; instances that were stopped stay stopped. If any guest refuses to shut down, or anything unexpected is going on, the hook fails and the refresh is aborted rather than forced.

- **When the daemon is not running**, the hook inspects the instance images directly; any suspend snapshot on disk (including one left behind by a previous daemon shutdown) blocks the refresh.

A few limitations to be aware of:

- Upgrading from a revision older than the first one shipping the `pre-refresh` hook is not protected by it; the hook can only run from a revision that already contains it.
- `snap revert` does not run the `pre-refresh` hook.
- When an unattended refresh is blocked, the old revision keeps running and the failure is recorded in `snap changes`; check `snap changes` and `snap change <id>` to see which instances blocked it. The trade-off is that the blocked refresh may delay other updates the change carried, which is accepted in preference to silently discarding guest memory state.
