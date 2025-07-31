#!/usr/bin/env python3
#
# Copyright (C) Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

"""Helper utilities for testing the Multipass CLI."""

import json
import re
import os
import shutil
import sys
import time
import uuid
import tempfile
import functools
import subprocess
import inspect
import site
import logging
from collections import defaultdict
from collections.abc import Sequence
from pprint import pformat
from contextlib import contextmanager
from pathlib import Path

import jq
import pexpect

if sys.platform == "win32":
    import pexpect.popen_spawn


from cli_tests.config import config


class JsonOutput(dict):
    """A type to store JSON command output."""

    def __init__(self, content, exitstatus):
        self.content = content
        self.exitstatus = exitstatus
        try:
            super().__init__(json.loads(content))
        except json.JSONDecodeError:
            super().__init__()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def __bool__(self):
        return self.exitstatus == 0

    def jq(self, query):
        """Real jq queries using the jq library"""
        return jq.compile(query).input(dict(self)).all()


class Output:
    """A type to store text command output."""

    def __init__(self, content, exitstatus):
        self.content = content
        self.exitstatus = exitstatus

    def __contains__(self, pattern):
        return re.search(pattern, self.content) is not None

    def __str__(self):
        return self.content

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def __bool__(self):
        return self.exitstatus == 0

    def json(self):
        """Cast output to JsonOutput."""
        return JsonOutput(self.content, self.exitstatus)


def retry(retries=3, delay=1.0):
    """Decorator to retry a function call based on return code"""

    def decorator(func):
        def wrapper(*args, **kwargs):
            for attempt in range(retries + 1):
                try:
                    result = func(*args, **kwargs)
                except pexpect.exceptions.TIMEOUT:
                    result = None
                if hasattr(result, "exitstatus") and result.exitstatus == 0:
                    return result
                if attempt < retries:
                    time.sleep(delay)
            return result

        return wrapper

    return decorator


def die(exit_code, message):
    """End testing process abruptly."""
    sys.stderr.write(f"\n❌🔥 {message}\n")
    # Flush both streams
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(exit_code)


def uuid4_str(prefix="", suffix=""):
    """Generate an UUID4 string, prefixed/suffixed with the given params."""
    return f"{prefix}{str(uuid.uuid4())}{suffix}"


def is_valid_ipv4_addr(ip_str):
    """Validate that given ip_str is a valid IPv4 address."""
    pattern = r"^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"
    return bool(re.match(pattern, ip_str))


def debug_interactive_shell(name):
    with multipass("shell", name, interactive=True) as shell:
        shell.interact()


def get_default_timeout_for(cmd):
    default_timeouts = {
        "delete": 30,
        "stop": 180,
        "launch": 600,
        "exec": 90,
        "start": 90,
        "mount": 180,
        "umount": 45,
    }
    if cmd in default_timeouts:
        return default_timeouts[cmd]
    return 10


def get_multipass_env():
    multipass_env = os.environ.copy()
    multipass_env["MULTIPASS_STORAGE"] = config.data_root
    return multipass_env


def get_multipass_path():
    return shutil.which("multipass", path=config.build_root)


def get_multipassd_path():
    return shutil.which("multipassd", path=config.build_root)


def run_as_privileged(py_func, *args, check=True, stdout=None, stderr=None):
    """
    Run a top-level function with elevated privileges using sudo.

    The function must be defined in an importable module (not __main__ or REPL).
    PYTHONPATH is set to include the module's root and the user's site-packages
    so that imports work correctly under sudo.

    Args:
        py_func: The function to run.
        *args: Arguments to pass to the function.
        check: If True, raise an error if the subprocess fails.
        stdout: Where to redirect standard output (default: inherit).
        stderr: Where to redirect standard error (default: inherit).
    """
    if not callable(py_func):
        raise ValueError("Expected a callable")

    def infer_pythonpath_root(module):
        depth = len(module.__name__.split(".")) - 1
        logging.debug(f"depth: {depth}")
        return str(Path(module.__file__).resolve().parents[depth])

    module = inspect.getmodule(py_func)
    if module is None or not hasattr(module, "__file__"):
        raise ValueError(
            "Function must come from an importable module (not REPL or __main__)"
        )

    module_name = module.__name__
    func_name = py_func.__name__
    module_root = infer_pythonpath_root(module)

    logging.debug(f"module.__file__ = {module.__file__}")
    logging.debug(f"inferred PYTHONPATH = {module_root}")

    # Positional args to string literals
    arg_strs = [repr(a) for a in args]
    # print('🔍 sys.path:', sys.path);
    full_expr = f"import sys; import {module_name}; {module_name}.{func_name}({', '.join(arg_strs)})"

    env = os.environ.copy()
    env["PYTHONPATH"] = (
        # Before running the executable, we need to add two things to PYTHONPATH:
        # 1) The test package's root directory
        # 2) Current user's site-packages
        # '2' is required since test dependencies are most likely installed at
        # the user level. If not, root/admin's site-packages are already accessible
        # to root by default.
        f"{module_root}{os.pathsep}{site.getusersitepackages()}{os.pathsep}{env.get('PYTHONPATH', '')}"
    )

    cmd = [
        get_sudo_tool(),
        # Ensure that the PYTHONPATH is propagated to the interpreter
        "--preserve-env" if sys.platform == "win32" else "--preserve-env=PYTHONPATH",
        sys.executable,
        "-c",
        full_expr,
    ]

    subprocess.run(cmd, check=check, stdout=stdout, stderr=stderr, env=env)


def authenticate_client_cert(data_root):
    if sys.platform == "win32":
        data_location = Path(
            os.getenv("LOCALAPPDATA", Path.home() / "AppData" / "Local")
        )
    elif sys.platform == "darwin":
        data_location = Path.home() / "Library" / "Application Support"
    else:
        # Not sure about this:
        data_location = Path.home() / ".local" / "share"
    # cat ~/snap/multipass/current/data/multipass-client-certificate/multipass_cert.pem | sudo tee -a /var/snap/multipass/common/data/multipassd/authenticated-certs/multipass_client_certs.pem > /dev/null
    # snap restart multipass
    src_path = data_location / "multipass-client-certificate" / "multipass_cert.pem"
    dst_path = (
        Path(data_root) / "data" / "authenticated-certs" / "multipass_client_certs.pem"
    )
    # Ensure the destination directory exists
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    # Ensure the file exists (like `tee -a`, which creates it if missing)
    dst_path.touch(exist_ok=True)
    with src_path.open("rb") as src, dst_path.open("ab") as dst:
        shutil.copyfileobj(src, dst)


def strip_ansi_escape(input):
    # https://stackoverflow.com/a/14693789/10872971
    ansi_escape_8bit = re.compile(
        r"""
        (?: # either 7-bit C1, two bytes, ESC Fe (omitting CSI)
            \x1B
            [@-Z\\-_]
        |   # or a single 8-bit byte Fe (omitting CSI)
            [\x80-\x9A\x9C-\x9F]
        |   # or CSI + control codes
            (?: # 7-bit CSI, ESC [
                \x1B\[
            |   # 8-bit CSI, 9B
                \x9B
            )
            [0-?]*  # Parameter bytes
            [ -/]*  # Intermediate bytes
            [@-~]   # Final byte
        )
    """,
        re.VERBOSE,
    )
    return ansi_escape_8bit.sub("", input)


class ContextManagerProxy:
    """Wrapper to avoid having to type multipass("command")() (note the parens)"""

    def __init__(self, callable):
        self.result = None
        self.callable = callable

    def _populate_result(self):
        if self.result is None:
            self.result = self.callable()

    def __getattr__(self, name):
        # Lazily run the command if someone tries to access attributes
        self._populate_result()
        return getattr(self.result, name)

    def __enter__(self):
        return self.callable.__enter__()

    def __exit__(self, *a):
        return self.callable.__exit__(*a)

    def __contains__(self, item):
        self._populate_result()
        return self.result.__contains__(item)

    def __bool__(self):
        self._populate_result()
        return self.result.__bool__()

    def __repr__(self):
        return strip_ansi_escape(self.result.content)


def multipass(*args, **kwargs):
    """Run a Multipass CLI command with optional retry, timeout, and context manager support.

    This function wraps Multipass CLI invocations using `pexpect`. It supports:
    - Retry logic on failure
    - Interactive vs. non-interactive execution
    - Context manager usage (`with` statement)
    - Lazy evaluation of command output

    Args:
        *args: Positional CLI arguments to pass to the `multipass` command.

    Keyword Args:
        timeout (int, optional): Maximum time in seconds to wait for command completion.Defaults to 30.
        interactive (bool, optional): If True, returns a live interactive `pexpect.spawn` object for manual interaction.
        retry (int, optional): Number of retry attempts on failure. Uses exponential backoff (2s delay).

    Returns:
        - If `interactive=True`: a `pexpect.spawn` object.
        - Otherwise, Output object containing the command's result.

    Raises:
        TimeoutError: If the command execution exceeds the timeout.
        RuntimeError: If the CLI process fails in a non-retryable way.

    Example:
        >>> out = multipass("list", "--format", "json").jq(".instances")
        >>> with multipass("info", "test") as output:
        ...     print(output.exitstatus)

    Notes:
        - You can access output fields directly: `multipass("version").exitstatus`
        - You can use `in` or `bool()` checks on the result proxy.
    """

    timeout = (
        kwargs.get("timeout")
        if "timeout" in kwargs
        else get_default_timeout_for(args[0])
    )
    echo = kwargs.get("echo") or False
    # print(f"timeout is {timeout} for {multipass_path} {' '.join(args)}")
    retry_count = kwargs.pop("retry", None)
    if retry_count is not None:

        @retry(retries=retry_count, delay=2.0)
        def retry_wrapper():
            return multipass(*args, **kwargs)

        return retry_wrapper()

    if config.print_cli_output:
        # Move to the next line since the CLI modifies the current line for
        # updating progress message
        sys.stderr.write("\n")

    if kwargs.get("interactive"):
        return pexpect.spawn(
            f"{get_multipass_path()} {' '.join(str(arg) for arg in args)}",
            logfile=(sys.stdout.buffer if config.print_cli_output else None),
            timeout=timeout,
            echo=echo,
            env=get_multipass_env(),
        )

    if sys.platform == "win32":

        class PopenCompatSpawn(pexpect.popen_spawn.PopenSpawn):
            def __init__(self, command, **kwargs):
                super().__init__(command, **kwargs)
                self.command = command if isinstance(command, list) else command.split()

            def isalive(self):
                return self.proc.poll() is None

            def terminate(self, wait=True, kill_on_timeout=True, timeout=5):
                if self.isalive():
                    self.proc.terminate()
                    if wait:
                        try:
                            self.proc.wait(timeout=timeout)
                        except subprocess.TimeoutExpired:
                            if kill_on_timeout:
                                self.proc.kill()

    class Cmd:
        """Run a Multipass CLI command and capture its output.

        Spawns a `pexpect` child process to run the given command,
        waits for it to complete, decodes the output, and ensures cleanup.

        Methods:
            __call__(): Returns an `Output` object with decoded output and exit status.
            __enter__(): Enables use as a context manager, returning the `Output`.
            __exit__(): No-op for context manager exit; always returns False.
        """

        def __init__(self):
            try:
                if not sys.platform == "win32":
                    self.pexpect_child = pexpect.spawn(
                        f"{get_multipass_path()} {' '.join(str(arg) for arg in args)}",
                        logfile=(
                            sys.stdout.buffer if config.print_cli_output else None
                        ),
                        timeout=timeout,
                        echo=echo,
                        env=get_multipass_env(),
                    )
                else:
                    self.pexpect_child = PopenCompatSpawn(
                        f"{get_multipass_path()} {' '.join(str(arg) for arg in args)}",
                        logfile=(
                            sys.stdout.buffer if config.print_cli_output else None
                        ),
                        timeout=timeout,
                        env=get_multipass_env(),
                    )

                self.pexpect_child.expect(pexpect.EOF, timeout=timeout)
                self.output_text = self.pexpect_child.before.decode("utf-8")
                self.pexpect_child.wait()
            except Exception as ex:
                print(ex)
                raise
            finally:
                if self.pexpect_child.isalive():
                    sys.stderr.write(
                        f"\n❌🔥 Terminating {get_multipass_path()} {' '.join(args)}\n"
                    )
                    sys.stdout.flush()
                    sys.stderr.flush()
                    self.pexpect_child.terminate()

        def __call__(self):
            return Output(self.output_text, self.pexpect_child.exitstatus)

        def __enter__(self):
            return self.__call__()

        def __exit__(self, *args):
            return False

    cmd = Cmd()
    return ContextManagerProxy(cmd)


def _retrieve_info_field(name, key):
    with multipass("info", "--format=json", f"{name}").json() as output:
        assert output
        return output["info"][name][key]


def state(name):
    """Retrieve state of a VM"""
    return _retrieve_info_field(name, "state")


def mounts(name):
    return _retrieve_info_field(name, "mounts")


def exec(name, *args, **kwargs):
    return multipass("exec", name, "--", *args, **kwargs)


def validate_output(*args, properties, jq_filter=None):
    with multipass(*args).json() as output:
        assert output.exitstatus == 0
        result = output.jq(jq_filter) if jq_filter else output
        assert len(result) == 1
        [instance] = result
        for k, v in properties.items():
            assert k in instance, f"Missing key '{k}' in instance:\n{pformat(instance)}"
            # A property can be assigned to a callable. In that case, we'd want
            # to treat it as a predicate and call it. We'd call it for each
            # element if the matching key's value is a collection.
            if callable(v):
                if isinstance(instance[k], Sequence):
                    for item in instance[k]:
                        assert v(item)
                else:
                    assert v(instance[k])
            else:
                assert instance[k] == v, (
                    f"\n❌ Assertion failed for key '{k}':\n"
                    f"  Expected: {v!r} (type: {type(v).__name__})\n"
                    f"  Actual:   {instance[k]!r} (type: {type(instance[k]).__name__})\n"
                )


@contextmanager
def TempDirectory(*args, **kwargs):
    with tempfile.TemporaryDirectory(*args, **kwargs) as tmp:
        yield Path(tmp)


def validate_list_output(name, properties):
    """Validate properties of a specific VM from `multipass list`.

    Fetches all VM data via `multipass list --format=json`, locates the
    VM by name, and asserts that its properties match the expected ones.

    Args:
        name (str): Name of the VM to validate.
        properties (dict): Expected properties. Values can be:
            - Literals (compared with equality)
            - Callables (used as predicates)
              - If the VM's value is a sequence, the predicate is applied to each item.

    Raises:
        AssertionError: If the VM is missing, or any property fails validation.
    """

    return validate_output(
        "list",
        "--format=json",
        properties=properties,
        jq_filter=f'.list[] | select(.name=="{name}")',
    )


def validate_info_snapshot_output(name, properties):
    """Validate properties of a specific VM from `multipass list`.

    Fetches all VM data via `multipass list --format=json`, locates the
    VM by name, and asserts that its properties match the expected ones.

    Args:
        name (str): Name of the VM to validate.
        properties (dict): Expected properties. Values can be:
            - Literals (compared with equality)
            - Callables (used as predicates)
              - If the VM's value is a sequence, the predicate is applied to each item.

    Raises:
        AssertionError: If the VM is missing, or any property fails validation.
    """

    return validate_output(
        "info",
        "--format=json",
        "--snapshots",
        f"{name}",
        properties=properties,
        jq_filter=f'.info["{name}"]',
    )


def validate_info_output(name, properties):
    return validate_output(
        "info",
        "--format=json",
        f"{name}",
        properties=properties,
        jq_filter=f'.info["{name}"]',
    )
    # # Verify the instance info
    # with multipass("info", "--format=json", f"{name}").json() as output:
    #     assert output.exitstatus == 0
    #     assert "errors" in output
    #     assert output["errors"] == []
    #     assert "info" in output
    #     assert name in output["info"]
    #     instance_info = output["info"][name]
    #     assert instance_info["cpu_count"] == "2"
    #     assert instance_info["snapshot_count"] == "0"
    #     assert instance_info["state"] == "Running"
    #     assert instance_info["mounts"] == {}
    #     assert instance_info["image_release"] == "24.04 LTS"
    #     assert is_valid_ipv4_addr(instance_info["ipv4"][0])


def file_exists(vm_name, *paths):
    """Return True if a file exists in the given VM, False otherwise."""
    return multipass("exec", f"{vm_name}", "--", "ls", *paths, timeout=180)


def take_snapshot(vm_name, snapshot_name, expected_parent="", expected_comment=""):
    """Create and verify a snapshot of a Multipass VM.

    - Creates a file inside the VM to mark pre-snapshot state.
    - Verifies file presence to confirm the VM is responsive.
    - Stops the VM and takes a snapshot.
    - Asserts the snapshot exists and matches expected metadata.

    Args:
        vm_name (str): Name of the target VM.
        snapshot_name (str): Name to identify the snapshot.
        expected_parent (str, optional): Expected parent snapshot name. Defaults to "".
        expected_comment (str, optional): Expected snapshot comment. Defaults to "".

    Raises:
        AssertionError: If any command fails or snapshot metadata does not match expectations.
    """
    assert multipass("exec", f"{vm_name}", "--", "touch", f"before_{snapshot_name}")
    assert multipass("exec", f"{vm_name}", "--", "ls", f"before_{snapshot_name}")
    assert multipass("stop", f"{vm_name}")

    with multipass("snapshot", f"{vm_name}", "--name", f"{snapshot_name}") as output:
        assert output.exitstatus == 0
        assert "Snapshot taken" in output

    with multipass("list", "--format=json", "--snapshots").json() as output:
        assert output.exitstatus == 0
        assert vm_name in output["info"]
        assert snapshot_name in output["info"][vm_name]
        snapshot = output["info"][vm_name][snapshot_name]
        assert expected_parent == snapshot["parent"]
        assert expected_comment == snapshot["comment"]


def build_snapshot_tree(vm_name, tree, parent=""):
    for name, children in tree.items():
        take_snapshot(vm_name, name, parent)
        build_snapshot_tree(vm_name, children, name)
        if parent != "":
            assert multipass("restore", f"{vm_name}.{parent}", "--destructive")


def collapse_to_snapshot_tree(flat_map):
    tree = {}
    children = defaultdict(dict)
    roots = []

    # First pass: map all nodes to their children
    for name, meta in flat_map.items():
        parent = meta["parent"]
        if parent:
            children[parent][name] = children[name]  # ensures nesting
        else:
            roots.append(name)
            tree[name] = children[name]

    return tree


def find_lineage(tree, target):
    def dfs(node, path):
        for key, child in node.items():
            new_path = path + [key]
            if key == target:
                return new_path
            result = dfs(child, new_path)
            if result:
                return result
        return None

    return dfs(tree, [])


@functools.cache
def get_sudo_tool():
    if sys.platform == "win32":
        sudo_tool = "gsudo"
        install_hint = "winget install gsudo"
    else:
        sudo_tool = "sudo"
        install_hint = "apt install sudo"

    result = shutil.which(sudo_tool)

    if not result:
        die(
            10,
            f"❌ `{sudo_tool}` is required but not found. Install it with: `{install_hint}`",
        )

    return result
