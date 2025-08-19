# Multipass CLI tests

TODO: Elaborate more.

## Installing deps

```bash

sudo apt install python3-pip

```

## To run

```bash
pytest -s tools/cli_tests/cli_tests.py --bin-dir build/bin/ --data-root=/tmp/multipass-test --remove-all-instances -vv
```

## Useful commands

```bash
# kill zombie dnsmasq
ps -eo pid,cmd | awk '/dnsmasq/ && /mpqemubr0/ { print $1 }' | xargs sudo kill -9

# kill multipassd
sudo kill -TERM $(pidof multipassd)
```

## Best practices

* If a test case takes >1m to complete, mark it with `@pytest.mark.slow`

## TL; DR;

```bash
sudo apt install python3-pip
# Install the latest stable multipass
sudo snap install multipass
# Clone the feature branch only
git clone https://github.com/canonical/multipass.git --single-branch --branch feature/cli-tests
cd multipass/
# Installs all the deps needed for the tests
pip install -e ./tools/cli_tests/ --break-system-packages
# Run the tests!
pytest -s tools/cli_tests --remove-all-instances --print-all-output --daemon-controller=snapd -vv
```
