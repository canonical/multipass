(reference-command-line-interface-wait-ready)=
# wait-ready

> See also: [`start`](reference-command-line-interface-start), [`launch`](reference-command-line-interface-launch)

The `multipass wait-ready` command blocks execution until the Multipass daemon is fully initialized and ready to accept requests. This is particularly useful in automated environments such as CI/CD pipelines, where subsequent Multipass commands may fail if the daemon has not yet finished starting up.

By waiting for the daemon to reach a ready state, scripts and services can safely perform operations such as launching or listing instances without encountering transient "daemon not ready" errors.

Use the `--timeout` option to limit how long the command waits for readiness before exiting with a non-zero status code.
For example, to wait up to 30 seconds for the daemon to become ready:

```bash
multipass wait-ready --timeout 30
```

---

The full `multipass help wait-ready` output explains the available options:

```{code-block} text

Usage: multipass wait-ready [options]
Wait for the Multipass daemon to be ready. This command will block until the
daemon has initialized, fetched up-to-date image information, and is ready to
accept requests. Its main use is to prevent failures caused by incomplete
initialization in batch operations. An optional timeout aborts the command if
reached.

Options:
  -h, --help           Displays help on commandline options
  -v, --verbose        Increase logging verbosity. Repeat the 'v' in the short
                       option for more detail. Maximum verbosity is obtained
                       with 4 (or more) v's, i.e. -vvvv.
  --timeout <timeout>  Maximum time, in seconds, to wait for the command to
                       complete. Note that some background operations may
                       continue beyond that. By default, instance startup and
                       initialization is limited to 5 minutes each.
```

## **Example usage:**

```bash
multipass wait-ready && multipass launch
```

This command ensures that the daemon is ready before launching an instance.

## **Notes:**

- `wait-ready` checks the daemon’s readiness via a gRPC endpoint.
- If the daemon is not running, `wait-ready` attempts to connect until it becomes available or the timeout elapses.
- The command exits with status `0` when the daemon is ready, and non-zero otherwise.
