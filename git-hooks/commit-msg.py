#!/usr/bin/env python3

"""
Git commit-msg hook to validate commit messages according to team guidelines.

Validates rules: 1-2, 4-6, 8, 10, and 12.
"""
import argparse
import re
import sys
import textwrap
import subprocess
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
        RULE10 =  "MSG10. Do not include more than 1 consecutive blank line, except in quoted text."
        RULE12 = ("MSG12. Wrap the body at 72 characters, except on lines consisting only of "
                  "blockquotes, references, sign-offs, and co-authors.")
        # fmt: on

    def __init__(self, msg, *, strict=False):
        """
        Perform validation of the specified commit message.

        If strict is False, allow autostash keywords in the commit message.
        """
        self.rules = [
            (self.Rules.RULE1, self.validate_rule1),
            (self.Rules.RULE2, self.validate_rule2),
            (self.Rules.RULE4, self.validate_rule4),
            (self.Rules.RULE5, self.validate_rule5),
            (self.Rules.RULE6, self.validate_rule6),
            (self.Rules.RULE8, self.validate_rule8),
            (self.Rules.RULE10, self.validate_rule10),
            (self.Rules.RULE12, self.validate_rule12),
        ]

        self.strict = strict
        self.enough = False

        self.msg = subprocess.check_output(
            ["git", "stripspace", "-s"], input=msg, text=True
        )

        self.lines = self.msg.splitlines() if msg else []
        self.subject = self.lines[0].rstrip() if self.lines else ""
        self.body = self.lines[1:]

        if not self.strict:
            # In non-strict mode, allow special keywords used by `git rebase --autosquash`
            self.subject = re.sub(r"^(fixup|squash)! ", "", self.subject)

        self.errors = self.validate_all()

    def is_inflexible_line(self, line):
        return bool(
            # Ignore quoted lines
            re.match("(>).*\n?", line)
            or
            # Ignore references
            re.match(r"\[\d+\]:.*\n?", line)
            or
            # Ignore Signed-off-by:
            re.match(r"Signed-off-by:.*\n?", line, re.IGNORECASE)
            or
            # Ignore Co-authored-by:
            re.match(r"Co-authored-by:.*\n?", line, re.IGNORECASE)
        )

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

    def validate_rule10(self):
        both_blank = lambda l1, l2: not l1 and not l2
        return not any(both_blank(l1, l2) for l1, l2 in zip(self.body, self.body[1:]))

    def validate_rule12(self):
        return all(len(line) <= 72 for line in self.body if not self.is_inflexible_line(line))


def validate(msg, *, strict=False):
    """
    Validate the commit message against the defined rules.

    Returns a list of error messages for any rule violations.
    """
    return CommitMsgRulesChecker(msg, strict=strict).errors


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


# Tests
class TestCommitMsgRulesChecker:
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
            textwrap.dedent(
                """\
                [fine] Just another one

                With interspersed blank lines.

                This is the last paragraph.
                """
            ),
        ]

        self._test_valid_msgs(valid_messages)

    def test_autosquash_keywords(self):
        autosquash_messages = [
            "fixup! [category] This is a valid subject line",
            "squash! [good] Another one",
        ]

        for msg in autosquash_messages:
            loose_checker = CommitMsgRulesChecker(msg)
            assert not loose_checker.errors, f"Loose validation should pass for: {msg!r} - Errors: {checker.errors}"

            strict_checker = CommitMsgRulesChecker(msg, strict=True)
            assert strict_checker.errors, f"Strict validation should fail for: {msg!r}"

    def test_rule1_subject_line_required_observed(self):
        valid_messages = [
            "[category] This is a valid subject line",
            "[good] Another one",
        ]

        for msg in valid_messages:
            self._test_rule("MSG1", msg, expect_failure=False)

    def test_rule1_subject_line_required_breached(self):
        invalid_messages = ["", "   ", "\n", "  \n", "\n  \n"]

        for msg in invalid_messages:
            self._test_rule("MSG1", msg, expect_failure=True)

    def test_rule2_category_format_observed(self):
        valid_messages = [
            "[fix] This and that",
            "[feature] XYZ",
            "[bug-fix] Fix bug",
            "[fix-issue-123] Fix bug in validation logic",
        ]

        for msg in valid_messages:
            self._test_rule("MSG2", msg, expect_failure=False)

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
            self._test_rule("MSG2", msg, expect_failure=True)

    def test_rule4_space_and_capitalization_observed(self):
        valid_messages = ["[film] Matrix", "[book] Pandora's Star\nScience fiction novel."]

        for msg in valid_messages:
            self._test_rule("MSG4", msg, expect_failure=False)

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
            self._test_rule("MSG4", msg, expect_failure=True)

    def test_rule5_subject_length_observed(self):
        valid_messages = [
            "[less] This line has less than 50 characters",
            "[almost] This line has close to the 50 characters",
            "[exactly] This line has the expected 50 characters",
        ]

        for msg in valid_messages:
            self._test_rule("MSG5", msg, expect_failure=False)

    def test_rule5_subject_length_breached(self):
        invalid_messages = [
            "[over] This line is slightly over the 50 characters",
            "[wrong] This line has more than the expected 50 characters",
        ]

        for msg in invalid_messages:
            self._test_rule("MSG5", msg, expect_failure=True)

    def test_rule6_no_period_at_end_observed(self):
        valid_messages = [
            "[fix] Update documentation",
            "[feature] Add new user authentication system",
            "[test] Improve test coverage for validation logic",
        ]

        for msg in valid_messages:
            self._test_rule("MSG6", msg, expect_failure=False)

    def test_rule6_no_period_at_end_breached(self):
        invalid_messages = [
            "[fix] Update documentation.",
            "[feature] Add new user authentication system.",
            "[test] Improve test coverage for validation logic.",
        ]

        for msg in invalid_messages:
            self._test_rule("MSG6", msg, expect_failure=True)

    def test_rule8_blank_line_after_subject_observed(self):
        valid_messages = [
            "[feature] Add new user authentication system\n\nThis is the body.",
            "[fix] Update documentation\n\nThis is the body of the commit message.\n\nBlah blah.",
        ]

        for msg in valid_messages:
            self._test_rule("MSG8", msg, expect_failure=False)

    def test_rule8_blank_line_after_subject_breached(self):
        invalid_messages = [
            "[fix] Update documentation\nThis is the body without a blank line",
            "[feature] Add new user authentication system\nThis is the body without\na blank line",
        ]

        for msg in invalid_messages:
            self._test_rule("MSG8", msg, expect_failure=True)

    def test_rule10_no_consecutive_blank_lines_observed(self):
        valid_messages = [
            "[msg] Subject line\n\nAnd a body.",
            "[msg] Subject line\n\nAnd a body with\n\nmultiple paragraphs.",
        ]

        for msg in valid_messages:
            self._test_rule("MSG10", msg, expect_failure=False)

    def test_rule10_no_consecutive_blank_lines_breached(self):
        invalid_messages = [
            "[msg] Subject line\n\n\nAnd a body with\n\nmultiple paragraphs.",
            "[msg] Subject line\n\nAnd a body with\n\n\nmultiple paragraphs.",
            "[msg] Subject line\n\nAnd a body with\n\n\n\nmultiple paragraphs.",
        ]

        for msg in invalid_messages:
            self._test_rule("MSG10", msg, expect_failure=False)

    def test_rule12_body_line_length_observed(self):
        valid_messages = [
            "[msg] Subject\n\n" "This body line is under 72 characters long.",
            "[msg] Subject\n\n"
            "This body line is exactly 72 characters long, as prescribed by the rule.",
            "[msg] Subject\n\n"
            "This body line is almost 72 characters long, but not quite there yet...",
        ]

        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    def test_rule12_body_line_length_breached(self):
        invalid_messages = [
            "[msg] Subject\n\n"
            "This body line is clearly over 72 characters long, so it is longer than expected.",
            "[msg] Subject\n\n"
            "This body line is barely over 72 characters long, surpassing the limit...",
        ]
        for msg in invalid_messages:
            self._test_rule("MSG12", msg, expect_failure=True)

    def test_rule12_body_line_comment_exceeds_72_chars(self):
        valid_messages = [
            textwrap.dedent("""\
                [msg] Subject

                # This body line (comment) is clearly over 72 characters long, so it is longer than expected.
            """),
            textwrap.dedent("""\
                [msg] Subject

                # This comment line is barely over 72 characters long, surpassing the limit...",
            """)
        ]
        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    def test_rule12_body_line_contains_verbatim_text(self):
        valid_messages = [
            textwrap.dedent("""\
                [msg] Subject

                > This body line (verbatim text) is clearly over 72 characters long and probably copied from somewhere else (such as a tool's output), so it is longer than expected.
            """),
            textwrap.dedent("""\
                [msg] Subject

                > This verbatim line is barely over 72 characters long, surpassing the limit...
            """),
        ]
        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    def test_rule12_body_line_contains_verbatim_text_with_regular_empty_lines_between(self):
        valid_messages = [
            textwrap.dedent("""\
                [msg] Subject

                > This body line (verbatim text) is clearly over 72 characters long and probably copied from somewhere else (such as a tool's output), so it is longer than expected.

                > This body line (verbatim text) is clearly over 72 characters long and probably copied from somewhere else (such as a tool's output), so it is longer than expected.

                Regular text
            """)
        ]
        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    def test_rule12_body_line_contains_verbatim_empty_lines(self):
        valid_messages = [
            textwrap.dedent("""\
                [msg] Subject

                > This body line (verbatim text) is clearly over 72 characters long and probably copied from somewhere else (such as a tool's output), so it is longer than expected."
                >
                >
                >

                Regular text
            """)
        ]
        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    def test_rule12_body_line_contains_footnote_references(self):
        valid_messages = [
            textwrap.dedent("""\
                [msg] Subject

                > This body line contains footnote references; [1] and [2]
                [1]: https://www.example.com/legolas-what-your-elf-eyes-see
                [2]: https://www.example.com/the-uruks-have-turned-northeast/they-are-taking-hobbits-to-the-isengard-gard-gard-gard-gard

                Regular text
            """)
        ]
        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    def test_rule12_body_line_contains_a_long_signed_off_by(self):
        valid_messages = [
            textwrap.dedent("""\
                [msg] Fellowship of the Ring

                > This body line contains footnote references; [1] and [2]
                Signed-Off-By: Aragorn II, King Elessar Telcontar, High King of the Reunited Kingdom of Gondor and Arnor. <aragorn@middleearth.localhost>
                Signed-Off-By: Legolas Greenleaf Thranduilion, Prince of the Woodland Realm. <legolas@middleearth.localhost>
                Signed-Off-By: Gimli, son of Glóin, of the House of Durin. <gimli@middleearth.localhost>
                Co-Authored-By: Samwise Gamgee, Banazîr Galbasi, Gardener of Frodo Baggins. <samgee@middleearth.localhost>
                Co-Authored-By: Frodo Baggins, Maura Labingi, Bearer of the One Ring. <fbaggins@middleearth.localhost>
                Co-Authored-By: Boromir, Captain and High Warden of the White Tower. <boromir@middleearth.localhost>

                Regular text
            """)
        ]
        for msg in valid_messages:
            self._test_rule("MSG12", msg, expect_failure=False)

    @staticmethod
    def _test_valid_msgs(valid_messages):
        for msg in valid_messages:
            checker = CommitMsgRulesChecker(msg)
            assert not checker.errors, f"Valid message failed: {msg!r} - Errors: {checker.errors}"

    @staticmethod
    def _test_rule(rule, msg, expect_failure, *, strict=False):
        checker = CommitMsgRulesChecker(msg, strict=strict)
        rule_broken = any(rule in error for error in checker.errors)
        error = f"Rule {rule} should {'fail' if expect_failure else 'pass'} for: {msg!r}"

        assert rule_broken == expect_failure, error + f" - Errors: {checker.errors}"


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


def main():
    """
    Entry point for the commit-msg hook.

    Assumes that commits are made with `--cleanup=strip` (when used as a hook).

    Supports --tests flag to run unit tests, otherwise expects a single argument:
    the path to the commit message file. Use a single dash (-) for stdin.
    """
    parser = argparse.ArgumentParser(description="Git commit message validator")
    parser.add_argument("--tests", action="store_true", help="Run unit tests")
    parser.add_argument("--strict", action="store_true", help="Enable strict validation checks")
    parser.add_argument("commit_msg_file", nargs="?", help="Path to commit message file")

    args = parser.parse_args()  # exits on error

    if args.tests:
        sys.exit(run_tests())

    if not args.commit_msg_file:
        print("Error: Expected a commit message file path", file=sys.stderr)
        sys.exit(1)

    try:
        if args.commit_msg_file == "-":
            msg = sys.stdin.read()
        else:
            commit_msg_file = Path(args.commit_msg_file)
            msg = commit_msg_file.read_text(encoding="utf-8")
    except Exception as e:
        print(f"Error reading commit msg: {e}", file=sys.stderr)
        sys.exit(2)

    sys.exit(handle_errors(validate(msg, strict=args.strict)))


if __name__ == "__main__":
    main()
