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
        return not self.body or self.body[0].isspace()

    def validate_rule9(self):
        return True  # TODO@no-merge

    def validate_rule10(self):
        both_blank = lambda l1, l2: l1.isspace() and l2.isspace()
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

    sys.exit(handle_errors(validate(msg)))


if __name__ == "__main__":
    main()
