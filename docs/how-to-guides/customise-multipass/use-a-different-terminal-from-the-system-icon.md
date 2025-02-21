(how-to-guides-customise-multipass-use-a-different-terminal-from-the-system-icon)=
# Use a different terminal from the system icon

> See also: [How to install Multipass](/how-to-guides/install-multipass), [`shell`](/reference/command-line-interface/shell).

If you want, you can change the terminal application used by the Multipass system menu icon.

```{note}
Currently available only for macOS
```

To do so, you need to tell macOS which terminal to use for the `.command` file type. This document presents two ways of achieving this.

## Using `duti`

[`duti`](https://github.com/moretension/duti/) is a small helper application that can modify the default application preferences. It's also [available from `brew`](https://formulae.brew.sh/formula/duti).

Find out your preferred terminal's bundle identifier using `mdls`:

```console
mdls /Applications/iTerm.app/ | grep BundleIdentifier
kMDItemCFBundleIdentifier              = "com.googlecode.iterm2"
```

And make it the default for script files using `duti`:

```console
duti -s com.googlecode.iTerm2 com.apple.terminal.shell-script shell
```

## Using Finder

Create an empty file with a `.command` extension and find it in Finder. Select the file and press `⌘I`. You should be presented with an information pane like this:

```{figure} /images/multipass-file-command-info.png
   :width: 289px
   :alt: file.command info
```

<!-- Original image on the Asset Manager
![file.command info|289x366](https://assets.ubuntu.com/v1/1ce425a9-multipass-file-command-info.png)
-->

Expand the "Open with:" section, select your preferred terminal application and click on "Change All...".

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://canonical.com/multipass/docs/changing-terminal" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*
