(reference-command-line-interface-transfer)=
# transfer

The `multipass transfer` command copies files between host and instance, without the need of mounting a folder on the instance.

For example, to copy a local file `local_file.txt` to the default home folder of the instance `good-prawn`, use the command:

```plain
multipass transfer local_file.txt good-prawn:.
```

Conversely, to copy a file `instance_file.txt` from the default home folder of the `ample-pigeon` instance to the current working folder, use the command:

```plain
multipass transfer ample-pigeon:remote_file.txt .
```

The source file can be the host standard input, in which case the stream will be written on the destination file on the instance. In the same way, the destination can be the standard output of the host and the source a file on the instance. In both cases, the standard input and output are specified with `-`.

If the target path does not exist, you can use the `--parents` option, that will create any missing parent directories. For example, if you run the command:

```plain
multipass transfer local_file.txt ample-pigeon:non/existent/path/remote_file.txt
```

and the system informs you that:

```plain
[2022-10-11T13:07:25.789] [error] [sftp] remote target does not exist
```

you can use the `--parents` option to create the missing parent folders:

```plain
multipass transfer --parents local_file.txt ample-pigeon:non/existent/path/remote_file.txt
```

You can also copy an entire directory tree using the `--recursive` option:

```plain
multipass transfer --recursive ample-pigeon:dir .
```

```{caution}Symbolic links are not followed during recursive transfer.```

---

The full `multipass help transfer` output explains the available options:
```plain
Usage: multipass transfer [options] <source> [<source> ...] <destination>
Copy files and directories between the host and instances.

Options:
  -h, --help       Displays help on commandline options
  -v, --verbose    Increase logging verbosity. Repeat the 'v' in the short
                   option for more detail. Maximum verbosity is obtained with 4
                   (or more) v's, i.e. -vvvv.
  -r, --recursive  Recursively copy entire directories
  -p, --parents    Make parent directories as needed

Arguments:
  source           One or more paths to transfer, prefixed with <name:> for
                   paths inside the instance, or '-' for stdin
  destination      The destination path, prefixed with <name:> for a path
                   inside the instance, or '-' for stdout
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/transfer-command" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

