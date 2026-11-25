#!/usr/bin/env python3
"""Synchronize CLI reference pages with `multipass help <command>` output.

Intended to run on Linux with a clean Multipass installation. The client inherits
the current environment, except for the locale; existing settings can affect help.
Source-built clients require their normal certificate and runtime setup.

Each command page in docs/reference/command-line-interface/<command>.md must
contain a single pair of HTML comment markers:

    <!-- begin help -->

    ```{code-block} text
    ...
    ```

    <!-- end help -->

Usage:
    ./tools/sync-cli-docs.py          # update out-of-sync CLI docs in place
    ./tools/sync-cli-docs.py --check  # verify CLI docs are up to date (for CI)
"""

from __future__ import annotations

import argparse
import difflib
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

BEGIN_MARKER = "<!-- begin help -->"
END_MARKER = "<!-- end help -->"

BEGIN_RE = re.compile(rf"^{re.escape(BEGIN_MARKER)}$", re.MULTILINE)
END_RE = re.compile(rf"^{re.escape(END_MARKER)}$", re.MULTILINE)
COMMAND_LINE_RE = re.compile(r"^  ([a-z][a-z0-9-]*) {2,}\S")

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DOCS_DIR = REPO_ROOT / "docs" / "reference" / "command-line-interface"
DEFAULT_MULTIPASS = REPO_ROOT / "build" / "bin" / "multipass"
COMMAND_TIMEOUT_SECONDS = 30


def resolve_multipass_binary(explicit: str | Path | None) -> Path:
    """Resolve the absolute path to the multipass CLI executable without dereferencing symlinks."""
    candidates = (explicit, ) if explicit is not None else (DEFAULT_MULTIPASS,
                                                            "multipass")
    for candidate in candidates:
        if path := shutil.which(candidate):
            return Path(path).absolute()

    searched = ", ".join(str(c) for c in candidates)
    raise FileNotFoundError(
        f"multipass executable not found or not executable (tried: {searched}); "
        "pass --multipass <path>")


def run_multipass(
    multipass_bin: Path,
    args: list[str],
    env: dict[str, str],
    timeout: float = COMMAND_TIMEOUT_SECONDS,
) -> str:
    """Run multipass with the given arguments and return stdout."""
    cmd = [str(multipass_bin), *args]
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        env=env,
        timeout=timeout,
        stdin=subprocess.DEVNULL,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"Command `{shlex.join(cmd)}` failed with exit code "
            f"{result.returncode}:\n{result.stderr}")
    if not result.stdout.strip():
        raise RuntimeError(f"Command `{shlex.join(cmd)}` produced no output.")
    return result.stdout


def parse_available_commands(help_output: str) -> set[str]:
    """Parse command names from the `Available commands:` section."""
    _, separator, section = f"\n{help_output}".partition(
        "\nAvailable commands:\n")

    if not separator:
        raise ValueError("Missing `Available commands:` section.")

    commands: set[str] = set()

    for line in section.rstrip().splitlines():
        if not (match := COMMAND_LINE_RE.match(line)):
            raise ValueError(f"Unexpected command line: {line!r}")

        cmd = match.group(1)
        if cmd in commands:
            raise ValueError(f"Duplicate command `{cmd}`.")

        commands.add(cmd)

    if not commands:
        raise ValueError("No available commands found.")

    return commands


def normalize_help_output(raw_output: str) -> str:
    """Strip trailing whitespace per line and surrounding blank lines."""
    return "\n".join(line.rstrip()
                     for line in raw_output.splitlines()).strip("\n")


def format_help_section(help_output: str) -> str:
    """Format the delimited help code block."""
    normalized = normalize_help_output(help_output)
    return f"{BEGIN_MARKER}\n\n```{{code-block}} text\n{normalized}\n```\n\n{END_MARKER}"


def replace_help_section(text: str, help_output: str, path: Path) -> str:
    """Replace the delimited help section in a markdown document."""
    begins = list(BEGIN_RE.finditer(text))
    ends = list(END_RE.finditer(text))

    if len(begins) != 1 or len(ends) != 1:
        raise ValueError(
            f"{path}: expected exactly 1 `{BEGIN_MARKER}` and 1 `{END_MARKER}` marker "
            f"(found {len(begins)} begin, {len(ends)} end).")

    begin_match = begins[0]
    end_match = ends[0]
    if begin_match.end() > end_match.start():
        raise ValueError(
            f"{path}: `{BEGIN_MARKER}` must appear before `{END_MARKER}`.")

    replacement = format_help_section(help_output)
    return text[:begin_match.start()] + replacement + text[end_match.end():]


def sync_docs(multipass_bin: Path, docs_dir: Path, check: bool) -> int:
    """Synchronize or check all CLI reference pages. Returns process exit code."""
    if not docs_dir.is_dir():
        raise FileNotFoundError(f"docs directory not found: {docs_dir}")

    changed_files: list[Path] = []

    env = {**os.environ, "LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"}
    general_help = run_multipass(multipass_bin, ["help"], env)
    commands = parse_available_commands(general_help)

    doc_files = {
        path.stem: path
        for path in docs_dir.glob("*.md") if path.name != "index.md"
    }

    missing_docs = sorted(commands - doc_files.keys())
    extra_docs = sorted(doc_files.keys() - commands)

    if missing_docs or extra_docs:
        raise ValueError(
            "CLI reference pages do not match available commands:\n"
            f"  Missing: {', '.join(missing_docs) or 'none'}\n"
            f"  Orphaned: {', '.join(extra_docs) or 'none'}")

    for cmd in sorted(commands):
        doc_path = doc_files[cmd]
        help_output = run_multipass(multipass_bin, ["help", cmd], env)
        original = doc_path.read_text(encoding="utf-8")
        updated = replace_help_section(original, help_output, doc_path)

        if updated != original:
            changed_files.append(doc_path)
            if check:
                diff = difflib.unified_diff(
                    original.splitlines(keepends=True),
                    updated.splitlines(keepends=True),
                    fromfile=str(doc_path),
                    tofile=f"{doc_path} (expected)",
                )
                sys.stdout.writelines(diff)
            else:
                doc_path.write_text(updated, encoding="utf-8")
                print(f"Updated {doc_path}")

    if check:
        if changed_files:
            print(
                f"ERROR: {len(changed_files)} CLI reference page(s) are out of date:\n"
                + "\n".join(f"  - {p}" for p in changed_files) +
                "\nRun `python3 tools/sync-cli-docs.py --multipass <multipass-path>` "
                "with the matching Multipass version, then commit the updated documentation.",
                file=sys.stderr,
            )
            return 1
        print(f"All {len(commands)} CLI reference docs are up to date.")
        return 0

    if changed_files:
        print(f"Updated {len(changed_files)} CLI reference page(s).")
    else:
        print(
            f"All {len(commands)} CLI reference docs are already up to date.")

    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=
        "Synchronize CLI reference docs with `multipass help <command>` output."
    )
    parser.add_argument(
        "--multipass",
        help=
        f"Path or command name for the multipass binary (default: {DEFAULT_MULTIPASS} or PATH).",
    )
    parser.add_argument(
        "--docs-dir",
        type=Path,
        default=DEFAULT_DOCS_DIR,
        help=
        f"Path to CLI reference docs directory (default: {DEFAULT_DOCS_DIR}).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help=
        "Check whether docs are synchronized without modifying files (exit 1 if out of sync).",
    )
    args = parser.parse_args(argv)

    multipass_bin = resolve_multipass_binary(args.multipass)
    print(f"Using Multipass executable: {multipass_bin}")
    return sync_docs(multipass_bin, args.docs_dir, check=args.check)


if __name__ == "__main__":
    sys.exit(main())
