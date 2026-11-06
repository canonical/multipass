"""GitHub Copilot CLI backend (`copilot -p`)."""

from __future__ import annotations

import json

from multiflow.backends.base import STEP_SERVER_NAME, Backend, StepInvocation


class CopilotBackend(Backend):
    name = "copilot"
    executable = "copilot"

    def command(self, inv: StepInvocation) -> tuple[list[str], dict[str, str]]:
        mcp_cfg = inv.step_dir / "mcp-copilot.json"
        mcp_cfg.write_text(json.dumps({
            "mcpServers": {
                STEP_SERVER_NAME: {"type": "local", "command": inv.mcp_command[0], "args": inv.mcp_command[1:], "tools": ["*"]}
            }
        }, indent=2))
        cmd = [
            self.executable, "-p", inv.prompt,
            "--allow-all-tools", "--no-ask-user", "-s",
            "--additional-mcp-config", f"@{mcp_cfg}",
            # The chat-mode server may be configured in the workspace; steps must not launch workflows.
            "--disable-mcp-server", "multiflow",
        ]
        if inv.model:
            cmd += ["--model", inv.model]
        denies: list[str] = []
        if inv.tools == "read-only":
            denies.append("write")
        else:
            denies += [f"write({p})" for p in inv.locked_paths]
        denies += inv.deny_extra
        for d in denies:
            cmd += ["--deny-tool", d]
        return cmd, {}
