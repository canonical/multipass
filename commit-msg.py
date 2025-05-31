#!/usr/bin/env python3
"""
Git commit-msg hook to validate commit messages according to team guidelines.
"""


def is_merge_commit(message):
    """
    Check if this is a merge commit.

    Commits are identified as merges iff they start with the word "Merge"
    """
    return message.startswith("Merge")
