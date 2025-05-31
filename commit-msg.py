#!/usr/bin/env python3

"""
Git commit-msg hook to validate commit messages according to team guidelines.
"""

import sys
from pathlib import Path


def main():
    """
    Entry point for the commit-msg hook.

    Expects a single argument beyond its own name: the path to the commit message file.
    """

    if len(sys.argv) != 2:
        print(f"Error: Expected a single argument, got {len(sys.argv) - 1}", file=sys.stderr)
        sys.exit(1)

    commit_msg_file = Path(sys.argv[1])

    try:
        message = commit_msg_file.read_text(encoding="utf-8")
    except Exception as e:
        print(f"Error reading commit message file: {e}", file=sys.stderr)
        sys.exit(2)

    sys.exit(handle_errors(validate_commit_message(message)))


def handle_errors(errors):
    if errors:
        print("Commit message validation failed:", file=sys.stderr)

        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    return 0


def is_merge_commit(message):
    """
    Check if this is a merge commit.

    Commits are identified as merges iff they start with the word "Merge"
    """
    return message.startswith("Merge")


def validate_commit_message(message):
    """Validate the entire commit message."""

    if is_merge_commit(message):
        return []

    lines = message.rstrip().split("\n")
    if not lines:
        return ["MSG1.\tBegin with a subject line."]

    errors = []
    return errors


if __name__ == "__main__":
    main()
