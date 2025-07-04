#!/usr/bin/env python3

"""
Git commit-msg hook to validate commit messages according to team guidelines.

Validates rules: 1-2, 4-6, 8-10, and 12.
"""
import argparse
import re
import sys
import textwrap
from enum import Enum
from pathlib import Path

# Regular expression to match the category in the subject line
CATEGORY_REGEX = r"^\[[a-z0-9]+(?:-[a-z0-9]+)*]"  # lowercase in square brackets, hyphenated allowed
SUBJECT_TAIL_REGEX = r"^\[.*]\s+[A-Z]\w*"  # a space and capitalized word follow the category


class CommitMsgRulesChecker:
    class Rules(Enum):
        # fmt: off
        RULE1 =   "MSG1.  Begin with a subject line."
        RULE2 =  ("MSG2.  Start the subject line with a lower-case, single-word category, within "
                  "square brackets (hyphenated, composite words are acceptable).")
        RULE4 =  ("MSG4.  Leave a single space after the category and capitalize the first "
                  "ensuing word.")
        RULE5 =   "MSG5.  Limit the subject line to 50 characters (category included)."
        RULE6 =   "MSG6.  Do not end the subject line with a period."
        RULE8 =   "MSG8.  If adding a body, separate it from the subject with a blank line."
        RULE9 =  ("MSG9.  Use multiple paragraphs in the body if needed. Separate them with a "
                  "blank line.")
        RULE10 =  "MSG10. Do not include more than 1 consecutive blank line."
        RULE12 =  "MSG12. Wrap the body at 72 characters."
        # fmt: on

    def __init__(self, msg):
        self.rules = [
            (self.Rules.RULE1, self.validate_rule1),
            (self.Rules.RULE2, self.validate_rule2),
            (self.Rules.RULE4, self.validate_rule4),
            (self.Rules.RULE5, self.validate_rule5),
            (self.Rules.RULE6, self.validate_rule6),
            (self.Rules.RULE8, self.validate_rule8),
            (self.Rules.RULE9, self.validate_rule9),
            (self.Rules.RULE10, self.validate_rule10),
            (self.Rules.RULE12, self.validate_rule12),
        ]

        self.enough = False
        self.msg = msg

        self.lines = self.msg.splitlines() if msg else []
        self.subject = self.lines[0].rstrip() if self.lines else ""
        self.body = self.lines[1:]

        self.errors = self.validate_all()

    def validate_all(self):
        if self.msg.lstrip().startswith("Merge"):
            return []

        return [rule.value for (rule, check) in self.rules if not self.enough and not check()]

    def validate_rule1(self):
        good = bool(
            self.msg and self.msg.strip() and self.lines and self.subject and self.subject.strip()
        )

        self.enough = not good
        return good

    def validate_rule2(self):
        return bool(re.match(CATEGORY_REGEX, self.subject))

    def validate_rule4(self):
        return bool(re.match(SUBJECT_TAIL_REGEX, self.subject))

    def validate_rule5(self):
        return len(self.subject) <= 50

    def validate_rule6(self):
        return not self.subject.endswith(".")

    def validate_rule8(self):
        return not self.body or not self.body[0]

    def validate_rule9(self):
        return True  # TODO@no-merge

    def validate_rule10(self):
        both_blank = lambda l1, l2: not l1 and not l2
        return not any(both_blank(l1, l2) for l1, l2 in zip(self.body, self.body[1:]))

    def validate_rule12(self):
        return all(len(line) <= 72 for line in self.body)


def validate(msg):
    """
    Validate the commit message against the defined rules.

    Returns a list of error messages for any rule violations.
    """
    return CommitMsgRulesChecker(msg).errors
    # return [rule.value for rule in checker.errors] if checker.errors else []


def handle_errors(errors):
    """
    Handle validation errors by printing and choosing a return: 1 if errors exist, 0 otherwise.
    """
    if errors:
        print("Commit message validation failed:", file=sys.stderr)

        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    return 0


def run_tests():
    """
    Run unit tests with pytest.
    """
    try:
        import pytest

        return pytest.main([__file__, "-v"])
    except ImportError:
        print(
            "Error: pytest is not installed. Please install it with: pip install pytest",
            file=sys.stderr,
        )
        return 1
    except Exception as e:
        print(f"Error running tests: {e}", file=sys.stderr)
        return 1


class TestCommitMsgRulesChecker:
    @staticmethod
    def _test_valid_msgs(valid_messages):
        for msg in valid_messages:
            checker = CommitMsgRulesChecker(msg)
            assert not checker.errors, f"Valid message failed: {msg!r} - Errors: {checker.errors}"

    def test_valid_single_line_commit_messages(self):
        """Test valid commit messages that should pass all rules."""

        valid_messages = [
            "[fix] Update documentation for API changes",
            "[feature] Add new user authentication system",
            "[test] Improve test coverage for validation logic",
        ]
        self._test_valid_msgs(valid_messages)

    def test_valid_multi_line_commit_messages(self):
        valid_messages = [
            textwrap.dedent(
                """\
                [good] A multi-line commit message

                A commit message with 2 paragraphs and some filler text in this one. The
                second paragraph can have multiple lines, but it should still be wrapped
                at 72 chars.
                """
            ),
            textwrap.dedent(
                """\
                [good] A commit message with multiple paragraphs

                Some filler text for the second paragraph. It can be longer than the
                first one.

                This is the third paragraph.
                """
            ),
        ]

        self._test_valid_msgs(valid_messages)

    def test_rule1_subject_line_required_breached(self):
        invalid_messages = ["", "   ", "\n", "  \n", "\n  \n", "\nasdf", "\n\nBody without subject"]

        for msg in invalid_messages:
            checker = CommitMsgRulesChecker(msg)
            assert any(
                "MSG1" in error for error in checker.errors
            ), f"MSG1 should fail for: {msg!r}"

    def test_rule1_subject_line_required_observed(self):
        valid_messages = [
            "[category] This is a valid subject line",
            "[good] Another one",
        ]

        for msg in valid_messages:
            checker = CommitMsgRulesChecker(msg)
            assert not any(
                "MSG1" in error for error in checker.errors
            ), f"MSG1 should pass for: {msg!r}"

    def test_rule2_category_format_breached(self):
        invalid_messages = [
            "fix Update documentation",
            "[Fix] Update documentation",
            "[fix[ Update documentation",
            "]fix] Update documentation",
            "[fix-Feature] Update docs",
            "[] Update documentation",
            "[fix_update] Documentation",
            "Update [fix] documentation",
            " [fix] Update documentation",
            "\t[fix] Update documentation",
            "foo\nbar",
            "foo\n[bar]",
        ]

        for msg in invalid_messages:
            checker = CommitMsgRulesChecker(msg)
            assert any(
                "MSG2" in error for error in checker.errors
            ), f"MSG2 should fail for: {msg!r}"

    def test_rule4_space_and_capitalization_breached(self):
        invalid_messages = [
            "[fix]update documentation",
            "[fix]  update documentation",
            "[fix] update documentation",
            "[fix]\nupdate documentation",
            "[fix]\tupdate documentation",
            "[fix]\t",
            "[fix] ",
            "[fix]",
        ]

        for msg in invalid_messages:
            checker = CommitMsgRulesChecker(msg)
            assert any(
                "MSG4" in error for error in checker.errors
            ), f"MSG4 should fail for: {msg!r}"


def main():
    """
    Entry point for the commit-msg hook.

    Supports --tests flag to run unit tests, otherwise expects a single argument:
    the path to the commit message file.
    """
    parser = argparse.ArgumentParser(description="Git commit message validator")
    parser.add_argument("--tests", action="store_true", help="Run unit tests")
    parser.add_argument("commit_msg_file", nargs="?", help="Path to commit message file")

    args = parser.parse_args()  # exits on error

    if args.tests:
        sys.exit(run_tests())  # TODO@no-merge get this to run in CI

    commit_msg_file = Path(args.commit_msg_file)
    if not args.commit_msg_file:
        print("Error: Expected a commit message file path", file=sys.stderr)
        sys.exit(1)

    try:
        msg = commit_msg_file.read_text(encoding="utf-8")
    except Exception as e:
        print(f"Error reading commit msg file: {e}", file=sys.stderr)
        sys.exit(2)

    sys.exit(handle_errors(validate(msg)))


if __name__ == "__main__":
    main()
