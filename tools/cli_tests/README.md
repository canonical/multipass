# Multipass CLI tests

TODO: Elaborate more.

## To run

```bash
pytest -s tools/cli_tests/cli_tests.py --build-root build/bin/ --data-root=/tmp/multipass-test --remove-all-instances -vv
```

## Useful commands

```bash
# kill zombie dnsmasq
ps -eo pid,cmd | awk '/dnsmasq/ && /mpqemubr0/ { print $1 }' | xargs sudo kill -9

# kill multipassd
sudo kill -TERM $(pidof multipassd)
```
