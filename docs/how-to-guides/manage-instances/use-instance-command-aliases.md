(how-to-guides-manage-instances-use-instance-command-aliases)=
# Use instance command aliases

> See also: [Alias](explanation-alias), [Instance](explanation-instance).

This guide demonstrates how to create, list, run and remove aliases for commands running inside an instance.

## Create an alias

> See also: [`alias`](reference-command-line-interface-alias)

To create an alias that runs a command on a given instance, use the [`alias`](reference-command-line-interface-alias) command. The code below uses this command to create an alias `lscc` that will run the command `ls` inside an instance `crazy-cat`:

```{code-block} text
multipass alias crazy-cat:ls lscc
```

After running this command, the alias `lscc` is defined as running the command `ls` on the instance `crazy-cat`. If the alias name `lscc` is omitted, the alias name defaults to the name of the command to run (`ls` in this case).

### Working directory mapping

By default, if the host folder on which you are running an alias is mounted on the instance, the working directory on the instance is changed to the mounted directory. To avoid this behaviour, use the option `--no-map-working-directory` when defining the alias; for example:

```{code-block} text
multipass alias crazy-cat:pwd pwdcc --no-map-working-directory
```

### Alias contexts

> See also: [`prefer`](reference-command-line-interface-prefer)

Contexts are sets of aliases. While one can safely work with one context, named `default`, contexts can be useful in some scenarios; for example, to define aliases with the same name in different instances.

You can switch to using another context with `multipass prefer secondary`. Then, defining an alias in the new context is done in the usual way. The name of the alias can coincide with an already defined one. For example, `multipass alias cozy-canary:ls lscc`.

## List the existing aliases

> See also:  [`aliases`](reference-command-line-interface-aliases)

To see the list of aliases defined so far, use the `multipass aliases` command.

The output will be similar to the following:

```{code-block} text
Alias   Instance     Command   Context      Working directory
lscc    crazy-cat    ls        default      map
pwdcc   crazy-cat    pwd       default      default
lscc    cozy-canary  ls        secondary*   map
```

The current context is marked with an asterisk.

The column `Working directory` tells us on which directory of the host the alias will be run. The value `default` means that the alias will be run in the instance default working directory (normally, `/home/ubuntu`). The value `map` means that, in case the host directory from which the user calls the alias is mounted on the instance, the alias will be run on the mounted directory on the instance. The value will be `default` only if the `--no-map-working-directory` argument is present at alias creation.

## Run an alias

There are two ways to run an alias:
* `multipass <alias>`
* `<alias>`

### `multipass <alias>`

The first way of running an alias is invoking it with `multipass <alias>`, for example:

```{code-block} text
multipass lscc
```

This command opens a shell into the instance `cozy-canary`, runs `ls` and returns to the host command line, as if it was an `exec` command. Since we have defined two aliases with the same name, `lscc`, the one in the current context will be run.

If you want to run an alias outside the current context, you can use a fully-qualified alias name:

```{code-block} text
multipass default.lscc
```

Arguments are also supported, provided you separate any options with `--`:

```{code-block} text
multipass lscc -- -l
```

or:

```{code-block} text
multipass default.lscc -- -l
```

### `<alias>`

The second way of running an alias is a two-step process:

1. Add the Multipass alias script folder to the system path.
2. Run the alias.

#### Add the Multipass alias script folder to the system path

The instructions to add the Multipass alias script folder to the system path are displayed the first time you create an alias, and vary for each platform. For instance, when you run the command:

```{code-block} text
multipass alias crazy-cat:ls lscc
```

the following output is displayed:

```{code-block} text
You'll need to add this to your shell configuration (.bashrc, .zshrc or so) for
aliases to work without prefixing with `multipass`:

PATH="$PATH:/home/user/snap/multipass/common/bin"
```

`````{tab-set}

````{tab-item} Linux

On Linux, you'll have to edit the shell configuration file. In most Linux distributions, the shell used by default is `bash`. You can configure this option by editing the file `.bashrc` in the user's home directory using a text editor; for example:

```{code-block} text
nano ~/.bashrc
```

You can modify the path by appending a line to the `.bashrc` file, such as:

```{code-block} text
export PATH="$PATH:/home/user/snap/multipass/common/bin"
```

```{note}
Remember to replace the correct folder, as indicated in the output of the Multipass command above, and to restart the shell when done.

If your shell is `zsh` and not `bash`, the file to modify is `.zshrc` instead of `.bashrc`. The procedure is the same.
```

````

````{tab-item} macOS

On macOS, you'll have to edit the shell configuration file. The shell used by default is `zsh`. You can configure this option by editing the file `.zshrc` in the user's home directory using a text editor.

You can modify the path by appending a line to the `.zshrc` file, such as:


```{code-block} text
export PATH="$PATH:/Users/<username>/Library/Application Support/multipass/bin"
```

```{note}
Remember to replace the correct folder, as indicated in the output of the Multipass command above, and to restart the shell when done.
```

````

````{tab-item} Windows

On Windows, to make the change permanent, use PowerShell to store the old system path, add the alias folder to it, and store the new path:

```{code-block} text
$old_path = (Get-ItemProperty -Path 'Registry::HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Environment' -Name PATH).path
$new_path = “$old_path;C:\Users\<user>\AppData\Local\Multipass\bin”
Set-ItemProperty -Path 'Registry::HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Environment' -Name PATH -Value $new_path
```

Don't forget to restart your terminal. The folder is now permanently added to your path, and Multipass can now run aliases just invoking their name.

````

`````

#### Run the alias

Once you've added the alias folder to the system path, you can run it directly from the command line, without the need to also mention `multipass`; for example:

```{code-block} text
lscc
```

or:

```{code-block} text
default.lscc
```

Since the path has been added to the system path, this command is equivalent to `multipass lscc`. Arguments are also supported, without the need for `--`:

```{code-block} text
lscc -l
```

## Remove an alias

> See also: [`unalias`](reference-command-line-interface-unalias)

Finally, to remove the alias `lscc`, run the following command:

```{code-block} text
multipass unalias lscc
```

or:

```{code-block} text
multipass unalias default.lscc
```

The `unalias` command accepts many arguments, specifying more than one alias to remove. For example, you can remove both aliases `lscc` and `pwdcc` at once:

```{code-block} text
multipass unalias lscc pwdcc
```

You can also use the `--all` option to remove all the defined aliases in the current context at once:

```{code-block} text
multipass unalias --all
```

```{note}
Aliases are also removed when the instance for which they were defined is deleted and purged. This means that `multipass delete crazy-cat --purge` will also remove the aliases `lscc` and `pwdcc`.
```

<!-- Discourse contributors
<small>**Contributors:** @luisp , @saviq , @tmihoc , @andreitoterman , @itecompro , @ricab , @gzanchi , @sharder996</small>
-->
