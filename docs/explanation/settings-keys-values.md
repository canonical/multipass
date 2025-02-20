(explanation-settings-keys-values)=
# Settings keys and values

> See also: [Settings](/reference/settings/index)

Multipass settings are organised in a tree structure, where each individual setting is identified by a unique **key** and takes on a single **value** at any given time.

## Settings keys

A **settings key** is a string in the form of a dot-separated path through the settings tree (such as `client.primary-name`). It specifies a path along the settings tree, from the root to a leaf. Thus, individual settings correspond to the leaves of the settings tree.

Conceptually, branches of the tree can be singled out with wildcards, to refer to multiple settings at once. For instance, `local.<instance-name>.*` designates the settings that affect a specific instance. Wildcards can also be used to select separate branches. For example `local.*.cpus` refers to the number of CPUs of Multipass instances.

## Settings values

A **settings value** is a string whose syntax (possible values/representations) and semantics (their interpretation) is determined by the setting in question.

Values often express common concepts (such as `true`, `false`, `42`, etc.) and are interpreted internally using the corresponding data types (such as boolean, integer, etc.). They can also be more complex (such as a key combination), but they are always specified and displayed through a string representation (for example: `Ctrl+Alt+U`).
