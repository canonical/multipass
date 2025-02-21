(reference-command-line-interface-umount)=
# umount

> See also: [`mount`](/reference/command-line-interface/mount), [Mount](/explanation/mount), [How to share data with an instance](/how-to-guides/manage-instances/share-data-with-an-instance).

The `umount` command without any options unmounts all previously defined mappings of local directories from the host to an instance.

You can also unmount only a specific mount if you specify the desired path.

---

The full `multipass help umount` output explains the available options:

```{code-block} text
Usage: multipass umount [options] <mount> [<mount> ...]
Unmount a directory from an instance.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.

Arguments:
  mount          Mount points, in <name>[:<path>] format, where <name> are
                 instance names, and optional <path> are mount points. If
                 omitted, all mounts will be removed from the named instances.
```
