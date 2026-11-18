(reference-command-line-interface-exec)=
# exec

The `exec` command runs the given commands inside the instance. The first argument is the instance to run the commands on, `--` optionally separates the `multipass` options from the rest - the command to run itself:

```{terminal}
:prompt: ">"

multipass exec primary -- uname -r

4.15.0-48-generic
```

You can pipe standard input and output to/from the command; for example:

```{terminal}
:prompt: ">"

multipass exec primary -- lsb_release -a | grep ^Codename:

No LSB modules are available.
Codename:       bionic
```

The `--` separator is required if you want to pass options to the command being run. Options to the `exec` command itself must be specified before `--`.

## Working directory

You can specify on which instance directory the command must be run in three different ways.

1. `--working-directory <dir>` tells Multipass that the command must be run in the folder `<dir>`. For example:

   ```{terminal}
   :prompt: ">"

   multipass exec arriving-pipefish --working-directory /home -- ls -a

   .  ..  ubuntu
   ```

   The `ls -a` command shows the contents of the `/home` directory, because it was run from there.

2. Look through the mounted folders first. In case we are running the alias on the host from a directory which is mounted on the instance, the command will be run on the instance from there. If the working directory is not mounted on the instance, the command will be run on the default directory on the instance. This is the default behaviour and no parameter must be specified for this mapping to happen.

3. Run the command directly in the default directory in the instance (usually, it is `/home/ubuntu`). The parameter to force this behaviour is `--no-map-working-directory`.

---

The full `multipass help exec` output explains the available options:

```{code-block} text
Usage: multipass exec [options] <name> [--] <command>
Run a command on an instance

Options:
  -h, --help                      Displays help on commandline options
  -v, --verbose                   Increase logging verbosity. Repeat the 'v' in
                                  the short option for more detail. Maximum
                                  verbosity is obtained with 4 (or more) v's,
                                  i.e. -vvvv.
  -d, --working-directory <dir>   Change to <dir> before execution
  -n, --no-map-working-directory  Do not map the host execution path to a
                                  mounted path

Arguments:
  name                            Name of instance to execute the command on
  command                         Command to execute on the instance
```

```{seealso}
{ref}`How to use instance command aliases <how-to-guides-manage-instances-use-instance-command-aliases>`, [Multipass `exec` and shells](explanation-multipass-exec-and-shells), [`shell`](reference-command-line-interface-shell)
```
