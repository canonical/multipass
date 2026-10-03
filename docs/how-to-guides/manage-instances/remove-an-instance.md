(how-to-guides-manage-instances-remove-an-instance)=
# Remove an instance

> See also: [Instance](explanation-instance)

This guide demonstrates how to remove an instance, either temporarily or permanently.

## Move an instance to the recycle bin

> See also: [`delete`](reference-command-line-interface-delete), [`recover`](reference-command-line-interface-recover)

To mark an instance as deleted, run:

```{code-block} text
multipass delete keen-yak
```

Now, if you run `multipass list` to list the instances, you will see that it is actually just marked for deletion (or to put it in other words, moved to the recycle bin):

```{code-block} text
Name                    State             IPv4             Release
keen-yak                DELETED           --               Not Available
```

You can move all instances to the recycle bin at once using the `--all` option:

```{code-block} text
multipass delete --all
```

Instances that have been marked as deleted can later be recovered; for example:

```{code-block} text
multipass recover keen-yak
```

If you try `multipass list` again, you'll see that the instance is no longer marked for deletion:

```{code-block} text
Name                    State             IPv4             Image
keen-yak                Stopped           --               Ubuntu 26.04 LTS
```

## Remove instances deleted by earlier versions

> See also: [`recover`](reference-command-line-interface-recover)

Earlier versions of Multipass only marked instances as `Deleted`, so they could be recovered later. If you still have instances in that state, you can recover them with `multipass recover`, or remove them for good with `multipass delete`:

```{code-block} text
multipass delete keen-yak
```

```{note}
The `purge` command is deprecated and will be removed in an upcoming release. Use `multipass delete <instance>` instead.
```

You can also use the `--purge` option to permanently delete an instance in a single command; for example:

```{code-block} text
multipass delete --purge keen-yak
```
