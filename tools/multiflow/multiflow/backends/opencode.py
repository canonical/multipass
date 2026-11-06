"""OpenCode backend (`opencode run`). Untested: config shape follows opencode's documented schema."""

from __future__ import annotations

import json

from multiflow.backends.base import STEP_SERVER_NAME, Backend, StepInvocation


class OpenCodeBackend(Backend):
    name = "opencode"
    executable = "opencode"

    def command(self, inv: StepInvocation) -> tuple[list[str], dict[str, str]]:
        cfg = {
            "$schema": "https://opencode.ai/config.json",
            "mcp": {STEP_SERVER_NAME: {"type": "local", "command": inv.mcp_command, "enabled": True}},
        }
        if inv.tools == "read-only":
            cfg["permission"] = {"edit": "deny"}
        cfg_path = inv.step_dir / "opencode.json"
        cfg_path.write_text(json.dumps(cfg, indent=2))
        cmd = [self.executable, "run"]
        if inv.model:
            cmd += ["--model", inv.model]
        cmd.append(inv.prompt)
        return cmd, {"OPENCODE_CONFIG": str(cfg_path)}
