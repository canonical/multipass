(how-to-guides-customise-multipass-integrate-with-windows-terminal)=
# How to integrate with Windows Terminal

If you are on Windows and you want to use [Windows Terminal](https://aka.ms/terminal), Multipass can integrate with it to offer you an automatic `primary` profile.

## Multipass profile

Currently, Multipass can add a profile to Windows Terminal for the {ref}`primary-instance`. When you open a Windows Terminal tab with this profile, you'll automatically find yourself in a primary instance shell. Multipass automatically starts or launches the primary instance if needed.

![Screenshot: primary shell|800x490, 85%](https://assets.ubuntu.com/v1/f875c1d3-multipass-windows-terminal-1.png)

## Install Windows Terminal

The first step is to [install Windows Terminal](https://github.com/microsoft/terminal#installing-and-running-windows-terminal). Once you have it along Multipass, you can enable the integration.

## Enable integration

Open a terminal (Windows Terminal or any other) and enable the integration with the following command:

```
multipass set client.apps.windows-terminal.profiles=primary
```

For more information on this setting, see: [`client.apps.windows-terminal.profiles`](/reference/settings/client-apps-windows-terminal-profiles). Until you modify it, Multipass will try to add the profile if it finds it missing. To remove the profile see {ref}`integrate-with-windows-terminal-revert` below.

## Open a Multipass tab

You can now open a "Multipass" tab to get a shell in the primary instance. That can be achieved by clicking the new-tab drop-down and selecting the Multipass profile:

![Screenshot: drop-down menu|800x490, 85%](https://assets.ubuntu.com/v1/d14d32d6-multipass-windows-terminal-2.jpeg)

That's it!

(integrate-with-windows-terminal-revert)=
## Revert

If you want to disable the profile again, you can do so with:

```
multipass set client.apps.windows-terminal.profiles=none
```

Multipass will then remove the profile if it exists.

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/windows-terminal-integration" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
