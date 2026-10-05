(how-to-guides-manage-instances-remove-an-instance)=
# Remove an instance

> See also: [Instance](explanation-instance)

This guide demonstrates how to remove an instance permanently.

## Delete an instance

> See also: [`delete`](reference-command-line-interface-delete)

To delete an instance, run:

```{code-block} text
multipass delete keen-yak
```

Multipass asks you to confirm. Answer `y` to proceed; the default answer is no, so pressing Enter cancels:

```{code-block} text
Instance 'keen-yak' will be deleted permanently. Would you like to proceed? [y/N]: y
keen-yak is deleted.
```

```{caution}
Deleted instances cannot be recovered.
```

You can delete all instances at once using the `--all` option:

```{code-block} text
multipass delete --all
```

To skip the confirmation, for example in a script, add the `--force` option:

```{code-block} text
multipass delete --force keen-yak
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
