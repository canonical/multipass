(explanation-multipass-exec-and-shells)=
# Multipass `exec` and shells

> See also: [`exec` command](/reference/command-line-interface/exec)

## How `exec` parses commands

When you call `multipass exec` from a shell, the command is first parsed by the shell you are in. The result of that parsing is what the multipass client sees in its argument list (`argv`).

For example, when `multipass exec primary -- ls ~` is entered on a Linux shell, the tilde is translated to the calling user's local home directory before the command is passed to Multipass. But this is not the case on a Windows PowerShell, because “~” does not have the same meaning there.

### Quoting

Quoting also depends on the calling shell. On most Linux and macOS shells, single quotes delimit a string that the shell passes verbatim to the program.

The Windows PowerShell doesn't treat single quotes this way. A program called with `'abc def'` there would get two arguments: `'abc` and `def'`. Double quotes can be used instead: `"abc def"`, but the string they delimit is subject to shell expansion. For example:

```{code-block} text
set USER=me
multipass exec -n rich-zorilla -- bash -c "echo %USER%"
```

The output will be: `me`.

With Multipass, this is seldom a problem, as expansions use a different syntax on Linux:

```{code-block} text
multipass exec -n rich-zorilla -- bash -c "echo $USER"
```

The output in this case is: `ubuntu`.

## How SSH parses commands

Multipass executes the command after `--` in the given instance as if there was no further shell in the middle (this is a simplification, as the reality is a little more complicated).

This is slightly different from what one would get with SSH. Consider the following command in a `bash` shell:

```{code-block} text
multipass exec mp-builder -- python3 -c 'import sys; print(sys.argv)' foo bar
```

whose output is: `['-c', 'foo', 'bar']`.

When using SSH, the entire command would need to be enclosed within quotes:

```{code-block} text
ssh -i /var/root/Library/Application\ Support/multipassd/ssh-keys/id_rsa ubuntu@192.168.66.34 python -c 'import sys; print(sys.argv)' foo bar
```

Sample output:

```{code-block} text
bash: -c: line 1: syntax error near unexpected token `sys.argv'
bash: -c: line 1: `python -c import sys; print(sys.argv) foo bar'
```

## Using a shell to parse commands

To overcome the above problem with `multipass exec`, one can still have another shell parse the command in the instance with `multipass exec`, it just needs to be called explicitly. For example:

```{code-block} text
multipass exec calm-woodcock -- sh -c 'ls -a ~'
```

Sample output:

```{code-block} text
.  ..  .bash_logout  .bashrc  .cache  .profile  .ssh
```

Similarly, on the Windows Command Prompt:

```{code-block} text
multipass exec calm-woodcock -- sh -c "ls -a ~"
```

Provided we use the appropriate quoting for the calling shell, this behaves the same regardless of the host platform. Without `sh -c`, it also fails on all platforms (although possibly in different ways, depending on whether or not the nested command is quoted). The inner-shell trick provides a more consistent cross-platform experience.

## Input/output redirection

The `multipass exec` command can be used together with piping to redirect input/output between commands run on the host and on the instance. For example, this writes the contents of the current directory on the host to a file called `save` in the instance `rich-zorilla`:

```{code-block} text
ls -la | multipass exec -n rich-zorilla -- bash -c "cat > save"
```

Conversely, this saves the contents of the home directory inside `rich-zorilla` to a file on the host:

```{code-block} text
multipass exec -n rich-zorilla -- bash -c "ls -la" | cat > save
```

## Other shell tricks

Other shell features can be combined with `multipass exec` for different effects. Here is an example using bash's here-strings:

```{code-block} text
multipass exec -n primary -- bash << EOF
> hostname
> whoami
> EOF
```

Sample output:

```{code-block} text
primary
ubuntu
```

And another using command substitution:

```{code-block} text
ping $(multipass exec rich-zorilla -- hostname -I)
```

Sample output:

```{code-block} text
PING 10.239.73.39 (10.239.73.39) 56(84) bytes of data.
64 bytes from 10.239.73.39: icmp_seq=1 ttl=64 time=0.371 ms
64 bytes from 10.239.73.39: icmp_seq=2 ttl=64 time=0.304 ms
64 bytes from 10.239.73.39: icmp_seq=3 ttl=64 time=0.439 ms
^C
--- 10.239.73.39 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2054ms
rtt min/avg/max/mdev = 0.304/0.371/0.439/0.055 ms
```
