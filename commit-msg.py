#!/usr/bin/env python3

"""
Git commit-msg hook to validate commit messages according to team guidelines.
"""

import sys


def main():
    """
    Entry point for the commit-msg hook.

    Expects a single argument beyond its own name: the path to the commit message file.
    """

    if len(sys.argv) != 2:
        print(f"Error: Expected a single argument, got {len(sys.argv) - 1}", file=sys.stderr)
        sys.exit(1)


def is_merge_commit(message):
    """
    Check if this is a merge commit.

    Commits are identified as merges iff they start with the word "Merge"
    """
    return message.startswith("Merge")


if __name__ == "__main__":
    main()
