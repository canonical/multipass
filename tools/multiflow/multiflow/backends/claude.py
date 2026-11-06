"""Claude Code backend (`claude -p`)."""

from __future__ import annotations

import json

from multiflow.backends.base import STEP_SERVER_NAME, Backend, StepInvocation

_WRITE_TOOLS = ("Edit", "Write", "MultiEdit", "NotebookEdit")


class ClaudeBackend(Backend):
    name = "claude"
    executable = "claude"

    def command(self, inv: StepInvocation) -> tuple[list[str], dict[str, str]]:
        mcp_cfg = inv.step_dir / "mcp-claude.json"
        mcp_cfg.write_text(json.dumps({
            "mcpServers": {
                STEP_SERVER_NAME: {"type": "stdio", "command": inv.mcp_command[0], "args": inv.mcp_command[1:]}
            }
        }, indent=2))
        cmd = [
            self.executable, "-p", inv.prompt,
            "--permission-mode", "bypassPermissions",
            "--mcp-config", str(mcp_cfg),
        ]
        if inv.model:
            cmd += ["--model", inv.model]
        denies: list[str] = []
        if inv.tools == "read-only":
            denies += list(_WRITE_TOOLS)
        else:
            # Claude permission rules use '//' for absolute paths.
            denies += [f"{t}(/{p})" for p in inv.locked_paths for t in _WRITE_TOOLS]
        denies += inv.deny_extra
        if denies:
            cmd += ["--disallowedTools", *denies]
        return cmd, {}
