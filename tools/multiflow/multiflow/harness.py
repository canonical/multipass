"""Detect which agent harness multiflow was dispatched from, to use it as the default backend.

Sources, most reliable first:
1. the MCP client name sent in `initialize` (chat server only), e.g. `copilot-cli`, `claude-code`, `Visual Studio Code`;
2. the nearest harness among the process's ancestors (environment variables leak into nested harnesses,
   e.g. `claude` started from a Copilot shell still has `COPILOT_CLI=1`; the closest ancestor does not);
3. harness environment variables;
4. `copilot`.
"""

from __future__ import annotations

import os
import subprocess
from pathlib import PurePath
from typing import Mapping, Optional

DEFAULT_BACKEND = "copilot"

# VS Code's chat is GitHub Copilot, so VS Code dispatches to the copilot backend.
_CLIENT_NAMES = (("claude", "claude"), ("opencode", "opencode"), ("copilot", "copilot"),
                 ("visual studio code", "copilot"), ("vscode", "copilot"))
_EXECUTABLES = {"claude": "claude", "copilot": "copilot", "opencode": "opencode",
                "code": "copilot", "code-insiders": "copilot"}
_INTERPRETERS = {"node", "nodejs", "bun", "deno"}
_ENV_VARS = (("CLAUDECODE", "claude"), ("CLAUDE_CODE_ENTRYPOINT", "claude"), ("OPENCODE", "opencode"),
             ("COPILOT_CLI", "copilot"))
_MAX_DEPTH = 32


def from_client_name(name: Optional[str]) -> Optional[str]:
    low = (name or "").lower()
    return next((backend for key, backend in _CLIENT_NAMES if key in low), None)


def from_argv(argv: list[str]) -> Optional[str]:
    """Backend for a process command line, e.g. ['/usr/bin/claude', ...] or ['node', '/usr/local/bin/copilot']."""
    if not argv:
        return None
    names = [PurePath(argv[0]).name.lower()]
    if names[0] in _INTERPRETERS and len(argv) > 1:
        names.append(PurePath(argv[1]).name.lower())
    for n in names:
        stem = n.removesuffix(".exe").removesuffix(".js").removesuffix(".mjs")
        if stem in _EXECUTABLES:
            return _EXECUTABLES[stem]
        if stem.startswith("code helper"):  # macOS VS Code helper processes
            return "copilot"
    return None


def _parent_and_argv(pid: int) -> Optional[tuple[int, list[str]]]:
    proc = f"/proc/{pid}"
    if os.path.isdir("/proc/self"):
        try:
            with open(f"{proc}/stat") as f:
                ppid = int(f.read().rsplit(")", 1)[1].split()[1])
            with open(f"{proc}/cmdline", "rb") as f:
                argv = [a.decode(errors="replace") for a in f.read().split(b"\0") if a]
            return ppid, argv
        except (OSError, ValueError, IndexError):
            return None
    try:  # macOS / BSD
        def ps(fmt: str) -> str:
            return subprocess.run(["ps", "-o", fmt, "-p", str(pid)], capture_output=True, text=True,
                                  timeout=2).stdout.strip()
        ppid, _, args = ps("ppid=,args=").partition(" ")
        # `comm` keeps executable paths with spaces intact (e.g. ".../Code Helper (Plugin)"); args gives node's script.
        return int(ppid), [ps("comm="), *args.split()[1:]]
    except (OSError, ValueError, subprocess.SubprocessError):
        return None


def from_ancestry(pid: Optional[int] = None) -> Optional[str]:
    pid = os.getppid() if pid is None else pid
    for _ in range(_MAX_DEPTH):
        if pid <= 1:
            return None
        info = _parent_and_argv(pid)
        if info is None:
            return None
        ppid, argv = info
        if (backend := from_argv(argv)) is not None:
            return backend
        pid = ppid
    return None


def from_env(env: Optional[Mapping[str, str]] = None) -> Optional[str]:
    env = os.environ if env is None else env
    return next((backend for var, backend in _ENV_VARS if env.get(var)), None)


def detect(client_name: Optional[str] = None) -> tuple[str, str]:
    """Return (backend, how it was determined)."""
    if (b := from_client_name(client_name)) is not None:
        return b, f"MCP client '{client_name}'"
    if (b := from_ancestry()) is not None:
        return b, "parent process"
    if (b := from_env()) is not None:
        return b, "environment"
    return DEFAULT_BACKEND, "default"
