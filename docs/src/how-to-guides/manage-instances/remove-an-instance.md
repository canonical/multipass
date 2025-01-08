# Remove an instance
> See also: [Instance](/explanation/instance)

This guide demonstrates how to remove an instance, either temporarily or permanently.

## Move an instance to the recycle bin

> See also: [`delete`](/reference/command-line-interface/launch), [`recover`](/reference/command-line-interface/recover)

To mark an instance as deleted, run:

```plain
multipass delete keen-yak
```

Now, if you run `multipass list` to list the instances, you will see that it is actually just marked for deletion (or to put it in other words, moved to the recycle bin):

```plain
Name                    State             IPv4             Release
keen-yak                DELETED           --               Not Available
```

You can move all instances to the recycle bin at once using the `--all` option:

```plain
multipass delete --all
```

Instances that have been marked as deleted can later be recovered; for example:

```plain
multipass recover keen-yak
```

If you try `multipass list` again, you'll see that the instance is no longer marked for deletion:

```plain
Name                    State             IPv4             Release
keen-yak                STOPPED           --               Ubuntu 18.04 LTS
```

## Remove an instance permanently

> See also: [`delete`](/reference/command-line-interface/launch), [`purge`](/reference/command-line-interface/purge)

If you want to get rid of all instances in `Deleted` status for good, you can purge them:

```plain
multipass delete keen-yak
multipass purge
```

```{caution}
The `purge` command does not take an argument. It will permanently remove all instances marked as `Deleted`.
```

You can also use the `--purge` option to permanently delete an instance in a single command; for example:

```plain
multipass delete --purge keen-yak
```

---

*Errors or typos? Topics missing? Hard to read? <a href="https://docs.google.com/forms/d/e/1FAIpQLSd0XZDU9sbOCiljceh3rO_rkp6vazy2ZsIWgx4gsvl_Sec4Ig/viewform?usp=pp_url&entry.317501128=https://multipass.run/docs/remove-an-instance" target="_blank">Let us know</a> or <a href="https://github.com/canonical/multipass/issues/new/choose" target="_blank">open an issue on GitHub</a>.*

