(reference-settings-client-gui-hotkey)=
# client.gui.hotkey

```{caution}
This setting has been removed from the CLI starting from Multipass version 1.14.
It is now only available in the [GUI client](/reference/gui-client).
```

> See also: [`get`](/reference/command-line-interface/get), [`set`](/reference/command-line-interface/set), [GUI client](/reference/gui-client)

## Key

`client.gui.hotkey`

## Description

A keyboard shortcut for the GUI to open a shell into the {ref}`primary-instance`.

## Possible values

A single case-insensitive sequence of keys, containing:

  * zero or more modifiers (such as `Ctrl`, `Alt`, `Cmd`, `Command`, `Opt`, etc.)
  * one non-modifier key (such as `u`, `4`, `.`, `Space`, `Tab`, `Pause`, `F3`). When key names have multiple words, quote and use spaces (for example: `"Print Screen"`).
  * (on macOS) alternatively, UTF-8 characters for Mac keys (such as ⌘, ⌥, ⇧, ⌃)
  * a plus (`+`) sign separating each alphabetic word (but not key symbols) from the next
  * the empty string (`""`) to disable the hotkey

```{caution}
**Caveats:**
- There are some limitations on what keys and combinations are supported, depending on multiple factors such as keyboard, mapping, and OS (e.g. `AltGr`, numpad, or media keys may or may not work; `shift+enter` is rejected).
- Some combinations may be grabbed by the OS before they reach multipass (e.g. `meta+a` may open the Applications, `ctrl+alt+f3` may move ttys).
```

## Examples

  * `multipass set client.gui.hotkey="Ctrl+Print Screen"`.
  * `multipass set client.gui.hotkey="⌃⇧Y"`.
  * `multipass set client.gui.hotkey=option+space`.
  * `multipass set client.gui.hotkey=""`

## Default value

*  `Ctrl+Alt+U` on Linux and Windows
* `⌥⌘U` on macOS
