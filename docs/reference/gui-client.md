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

- {ref}`gui-client-catalogue-tab`
- {ref}`gui-client-instances-tab`
- {ref}`gui-client-settings-tab`

as well as a {ref}`gui-client-tray-icon` menu.

(gui-client-catalogue-tab)=
## Catalogue tab

Here you can browse the available Ubuntu images. The output is equivalent to the one given by `multipass find --only-images`. Blueprints are not yet available.

```{figure} /images/gui-client/multipass-gui-catalogue-tab.png
   :width: 720px
   :alt: GUI client - Catalogue tab
```

<!-- Original image on the Asset Manager
![Catalogue page](https://assets.ubuntu.com/v1/1edb2dfb-multipass-gui-catalogue-tab.png)
-->

You can configure an instance's launch options, specifying parameters such as its name, allocated resources and mounts.

```{figure} /images/gui-client/multipass-gui-configure-instance.png
   :width: 720px
   :alt: GUI client - Configure instance 
```

<!-- Original image on the Asset Manager
![Configure instance page](https://assets.ubuntu.com/v1/6a239e67-multipass-gui-configure-instance.png)
-->

When you launch a VM, you can see details on all the steps taken and be notified of errors.

```{figure} /images/gui-client/multipass-gui-launching-instance.png
   :width: 720px
   :alt: GUI client - Launching instance
```

<!-- Original image on the Asset Manager
![Launching page](https://assets.ubuntu.com/v1/17f00d22-multipass-gui-launching-instance.png)
-->

(gui-client-instances-tab)=
## Instances tab

Here you can see an overview of all your instances and perform bulk actions such as starting, stopping, suspending or deleting the selected ones. You can also filter instances by name and by state ("running" or "stopped").

```{figure} /images/gui-client/multipass-gui-instances-tab.png
   :width: 720px
   :alt: GUI client - Instances tab
```

<!-- Original image on the Asset Manager
![List of all instances page](https://assets.ubuntu.com/v1/909fad4d-multipass-gui-instances-tab.png)
-->

You can perform actions on an individual instance, such as starting, stopping, suspending or deleting it. You can also open shells within a running instance, where you can do all of your work that is specific to that instance.

```{figure} /images/gui-client/multipass-gui-instance.png
   :width: 720px
   :alt: GUI client - Instance shell
```

<!-- Original image on the Asset Manager
![Instance shell page](https://assets.ubuntu.com/v1/740d7ab4-multipass-gui-instance.png)
-->

```{caution}
Please note that when you delete an instance using the GUI client, Multipass removes it permanently and they cannot be recovered. This behaviour is equivalent to running the [`multipass delete --purge`](/reference/command-line-interface/delete) command.
```

You can also edit an instance; in particular, you can change its allocated resources, connect it to a bridged network or edit its mounts. Some of these settings require the instance to be stopped before they can be applied.

```{figure} /images/gui-client/multipass-gui-instance-edit.png
   :width: 720px
   :alt: GUI client - Edit instance
```

<!-- Original image on the Asset Manager
![Edit instance page](https://assets.ubuntu.com/v1/38a180c4-multipass-gui-instance-edit.png)
-->

(gui-client-settings-tab)=
## Settings tab

Here you can change various Multipass settings, although not all settings that are available in the CLI are present in the GUI and vice versa.

```{figure} /images/gui-client/multipass-gui-settings-tab.png
   :width: 720px
   :alt: GUI client - Settings tab
```

<!-- Original image on the Asset Manager
![Settings page](https://assets.ubuntu.com/v1/4ad40d35-multipass-gui-settings-tab.png)
-->

(gui-client-tray-icon)=
## Tray icon

You can manage your instances using the tray icon menu as well.

```{figure} /images/gui-client/multipass-gui-tray-icon-menu.png
   :width: 419px
   :alt: GUI client - Tray icon menu
```

<!-- Original image on the Asset Manager
![Tray icon menu](https://assets.ubuntu.com/v1/7e16f6bd-multipass-gui-tray-icon-menu.png)
-->

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/multipass-gui-client" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
