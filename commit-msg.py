#!/usr/bin/env python3

"""
Git commit-msg hook to validate commit messages according to team guidelines.

Validates rules: 1-2, 4-6, 8-10, and 12.
"""
import re
import sys
from enum import Enum
from pathlib import Path

# Regular expression to match the category in the subject line
CATEGORY_REGEX = r"^\[[a-z0-9]+(?:-[a-z0-9]+)*]"  # lowercase in square brackets, hyphenated allowed
SUBJECT_TAIL_REGEX = r"^\[.*]\s+[A-Z]\w+"  # a space and capitalized word follow the category


class CommitMsgRulesChecker:
    class Rules(Enum):
        # fmt: off
        RULE1 =   "MSG1.  Begin with a subject line."
        RULE2 =  ("MSG2.  Start the subject line with a lower-case, single-word category, within "
                  "square brackets (hyphenated, composite words are acceptable).")
        RULE4 =   ("MSG4.  Leave a single space after the category and capitalize the first "
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

        self.errors = []
        self.enough = False
        self.msg = msg

        self.lines = self.msg.splitlines() if msg else []
        self.subject = self.lines[0].rstrip() if self.lines else ""
        self.body = self.lines[1:]

        self.validate_all()

    def validate_all(self):
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
        return not self.body or self.body[0].isspace()

    def validate_rule9(self):
        return False

    def validate_rule10(self):
        return False

    def validate_rule12(self):
        return False


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
        msg = commit_msg_file.read_text(encoding="utf-8")
    except Exception as e:
        print(f"Error reading commit msg file: {e}", file=sys.stderr)
        sys.exit(2)

    sys.exit(handle_errors(validate_commit_message(msg)))


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


def is_merge_commit(msg):
    """
    Check if this is a merge commit.

    Commits are identified as merges iff they start with the word "Merge"
    """

    return msg.startswith("Merge")


def validate_commit_message(msg):
    """Validate the entire commit message."""

    if is_merge_commit(msg):
        return []

    lines = msg.rstrip().split("\n")
    if not lines:
        return ["MSG1.\tBegin with a subject line."]


def validate_subject_line(subject):
    """Validate the subject line according to rules 1-2, 4-6."""

    if not subject:
        return [Rules.RULE1]

    errors = []

    category_match = re.match(CATEGORY_REGEX, subject)
    if not category_match:
        errors.append(
            "Rule 2: Subject must start with [category] where category is lowercase (hyphens allowed)"
        )
        return errors  # Can't check other rules without proper category

    category = category_match.group(0)
    rest_of_subject = subject[len(category) :].lstrip()

    # Rule 4: Capitalize first word after category
    if rest_of_subject and not rest_of_subject[0].isupper():
        errors.append("Rule 4: First word after category must be capitalized")

    # Rule 5: Limit subject to 50 characters
    if len(subject) > 50:
        errors.append(f"Rule 5: Subject line too long ({len(subject)}/50 characters)")

    # Rule 6: Do not end subject with period
    if subject.endswith("."):
        errors.append("Rule 6: Subject line must not end with a period")

    return errors


def validate_body_format(lines):
    """Validate body formatting according to rules 8, 10, and 12."""
    errors = []

    if len(lines) == 1:
        # Only subject line, no body - this is valid
        return errors

    # Rule 8: If body exists, must be separated by blank line
    if len(lines) > 1 and lines[1] != "":
        errors.append("Rule 8: Subject and body must be separated by a blank line")

    # Rule 10: No more than 1 consecutive blank line
    prev_blank = False
    for i, line in enumerate(lines[1:], 1):  # Skip subject line
        if line == "":
            if prev_blank:
                errors.append(
                    f"Rule 10: Line {i+1} - No more than 1 consecutive blank line allowed"
                )
                break  # Report once per msg
            prev_blank = True
        else:
            prev_blank = False

            # Rule 12: Wrap body at 72 characters
            if len(line) > 72:
                errors.append(f"Rule 12: Line {i+1} exceeds 72 characters ({len(line)} chars)")

    return errors


if __name__ == "__main__":
    main()
