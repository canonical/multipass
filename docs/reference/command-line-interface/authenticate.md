(reference-command-line-interface-authenticate)=
# authenticate

> See also: [Authentication](/explanation/authentication), [How to authenticate users with the Multipass service](how-to-guides-customise-multipass-authenticate-users-with-the-multipass-service), [`local.passhprase`](/reference/settings/local-passphrase)

The `authenticate` command is used to authenticate a client with the Multipass service. Once authenticated, the client can issue commands such as `list`, `launch`, etc.

To help reduce the amount of typing for `authenticate`, one can also use `multipass auth` as an alias:

```{code-block} text
multipass auth foo
```

If no passphrase is specified in the `multipass authenticate` command line, you will be prompted to enter it.

---

The full `multipass help authenticate` output explains the available options:

```{code-block} text
Usage: multipass authenticate [options] [<passphrase>]
Authenticate with the Multipass service.
A system administrator should provide you with a passphrase
to allow use of the Multipass service.

Options:
  -h, --help     Displays help on commandline options
  -v, --verbose  Increase logging verbosity. Repeat the 'v' in the short option
                 for more detail. Maximum verbosity is obtained with 4 (or more)
                 v's, i.e. -vvvv.

Arguments:
  passphrase     Passphrase to register with the Multipass service. If omitted,
                 a prompt will be displayed for entering the passphrase.
```
