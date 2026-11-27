# /// script
# requires-python = ">=3.12"
# dependencies = [
#   "pyyaml",
# ]
# ///

"""Fail unless every sbomber request recorded in its statefile succeeded.

sbomber's `submit` records upload errors in the statefile but still exits 0, and
`poll` only looks at pending requests, so a run can pass without producing any
report. Adapted from canonical/valkey-artifacts/.github/scripts/check-sbomber-state.sh.

Run with `uv run tools/ssdlc/check_sbomber_state.py [STATEFILE]`.
"""

import argparse
import os
import sys
from pathlib import Path

import yaml

SUCCESS = "Succeeded"
NOT_STARTED = "Not started"  # omitted from the statefile, as it is the default


def collect_results(state: dict) -> list[tuple[str, str, str]]:
    """Return (artifact, client, status) for every request sbomber should have made."""
    enabled_clients = [
        name
        for name, config in (state.get("clients") or {}).items()
        if config is not None
    ]
    results = []
    for artifact in state.get("artifacts") or []:
        processing = artifact.get("processing") or {}
        for client in artifact.get("clients") or enabled_clients:
            status = (processing.get(client) or {}).get("status", NOT_STARTED)
            results.append((artifact["name"], client, status))
    return results


def write_step_summary(results: list[tuple[str, str, str]]) -> None:
    summary_path = os.environ.get("GITHUB_STEP_SUMMARY")
    if not summary_path:
        return
    with open(summary_path, "a", encoding="utf-8") as summary:
        summary.write(
            "### sbomber results\n\n| Artifact | Client | Status |\n|---|---|---|\n"
        )
        summary.writelines(
            f"| `{name}` | {client} | {status} |\n" for name, client, status in results
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "statefile",
        type=Path,
        nargs="?",
        default=Path(".statefile.yaml"),
        help="sbomber statefile (default: .statefile.yaml)",
    )
    args = parser.parse_args()

    if not args.statefile.is_file():
        print(
            f"error: no sbomber statefile at {args.statefile}; did 'prepare' run?",
            file=sys.stderr,
        )
        return 1

    results = collect_results(yaml.safe_load(args.statefile.read_text()) or {})
    for name, client, status in results:
        mark = "ok" if status == SUCCESS else "FAILED"
        print(f"{name:<34} {client:<8} {status:<12} {mark}")
    write_step_summary(results)

    if not results:
        print(
            "error: no requests found in the statefile; nothing was scanned.",
            file=sys.stderr,
        )
        return 1

    failed = sum(status != SUCCESS for _, _, status in results)
    if failed:
        print(
            f"error: {failed} of {len(results)} sbomber request(s) did not succeed; "
            "see the sbomber step output for the service response.",
            file=sys.stderr,
        )
        return 1

    print(f"All {len(results)} sbomber request(s) succeeded.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
