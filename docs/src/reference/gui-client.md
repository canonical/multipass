(reference-gui-client)=
# GUI client

> See also: [Instance](/explanation/instance), [Service](/explanation/service),  [Settings](/reference/settings/index)

```{caution}
The GUI was introduced in Multipass version 1.14. It is still in its MVP (Minimum Viable Product) stage, so it is likely to see some changes in design as it evolves.
```

The Multipass GUI (Graphical User Interface) is an application that acts as a client for interacting with the Multipass service. It aims to make the process of managing instances easier for users who do not want to interact with the CLI (Command Line Interface) client.
You can launch the GUI either using your system's application launcher or by running `multipass.gui` in a terminal.

> For more information on GUI logs, see [How to access logs](/how-to-guides/troubleshoot/access-logs).

As of now, the Multipass GUI provides the following set of capabilities, grouped under four tabs:
- [GUI client](#gui-client)
  - [Catalogue tab](#catalogue-tab)
  - [Instances tab](#instances-tab)
  - [Settings tab](#settings-tab)
  - [Tray icon](#tray-icon)

as well as a [tray icon](#tray-icon) menu.

## Catalogue tab

Here you can browse the available Ubuntu images. The output is equivalent to the one given by `multipass find --only-images`. Blueprints are not yet available.

![Catalogue page](https://lh3.googleusercontent.com/d/1DIbo-K2O83KHvESVvkI-QolsoYcMc-HV)

You can configure an instance's launch options, specifying parameters such as its name, allocated resources and mounts.

![Configure instance page](https://lh3.googleusercontent.com/d/1pdJ4A0M8Yn8rp921gfivHU4VLPG7WAAk)

When you launch a VM, you can see details on all the steps taken and be notified of errors.

![Launching page](https://lh3.googleusercontent.com/d/1fbmfomhTXcOjv22uYvvInhPh6oruTsbd)

## Instances tab

Here you can see an overview of all your instances and perform bulk actions such as starting, stopping, suspending or deleting the selected ones. You can also filter instances by name and by state ("running" or "stopped").

![List of all instances page](https://lh3.googleusercontent.com/d/15gDHhSDxcxYxIpJGiHTKYgRemzChk0NF)

You can perform actions on an individual instance, such as starting, stopping, suspending or deleting it. You can also open shells within a running instance, where you can do all of your work that is specific to that instance.

![Instance shell page](https://lh3.googleusercontent.com/d/1vgF-Cezrhz8h_7PZemOoU3MaR1KgElJR)

```{caution}
Please note that when you delete an instance using the GUI client, Multipass removes it permanently and they cannot be recovered. This behaviour is equivalent to running the [`multipass delete --purge`](/reference/command-line-interface/delete) command.
```

You can also edit an instance; in particular, you can change its allocated resources, connect it to a bridged network or edit its mounts. Some of these settings require the instance to be stopped before they can be applied.

![Edit instance page](https://lh3.googleusercontent.com/d/1vvZQUHoOJRWStjF3fALffk-jYy4N_WRi)

## Settings tab

Here you can change various Multipass settings, although not all settings that are available in the CLI are present in the GUI and vice versa.

![Settings page](https://lh3.googleusercontent.com/d/1Wkc_yUvIc2BpBufiYRcQsOD9dPnghxdc)

## Tray icon

You can manage your instances using the tray icon menu as well.

![Tray icon menu](https://lh3.googleusercontent.com/d/1E8ej8AR7j9maHuqp9EA-vFDi_ojQnTgP)

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/multipass-gui-client" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

