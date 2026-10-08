#!/bin/bash

# Non-interactive alternative to chatting with the CLI Behavior Test
# Generator agent by hand: takes an already-approved Gherkin `.feature` file
# and invokes `copilot` once to add the matching pytest scenario(s) to the
# tests/cli suite -- either into an existing, pertinent file (e.g. adding a
# mount test to cli_mount_test.py) or, if none fits, a new, aptly-named file
# (e.g. cli_mount_classic_test.py). The output is never named after the
# issue/PR (no cli_issue_<N>_test.py) unless no better name applies.
# Nothing else happens after that -- no git, no cat. The script detects
# which file was touched, compile-checks it, and reports its path, then
# stops.
#
# Usage: ./generate-test.sh --feature path/to/file.feature [--prompt "extra instructions"]
# Options:
#   --feature PATH   Path to the approved .feature file (required), e.g.
#                     tests/cli/issue_1234.feature
#   --prompt TEXT     Extra free-text guidance for the test implementation,
#                     e.g. "use the `instance` fixture", "mark it slow".
#                     Optional.
#   --test-path PATH  Force the output test file path (default: let the
#                     model pick a pertinent existing file to extend, or a
#                     new aptly-named one if none fits).
#   -h, --help        Show this help message.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

FEATURE_PATH=""
EXTRA_PROMPT=""
TEST_PATH=""

usage() {
  sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --feature) FEATURE_PATH="$2"; shift 2 ;;
    --prompt) EXTRA_PROMPT="$2"; shift 2 ;;
    --test-path) TEST_PATH="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

if [[ -z "$FEATURE_PATH" ]]; then
  echo "Error: --feature is required." >&2
  usage >&2
  exit 1
fi

if ! command -v copilot >/dev/null 2>&1; then
  echo "Error: 'copilot' (GitHub Copilot CLI) is required but not found in PATH." >&2
  exit 1
fi

ABS_FEATURE_PATH="$REPO_ROOT/$FEATURE_PATH"
if [[ "$FEATURE_PATH" = /* ]]; then
  ABS_FEATURE_PATH="$FEATURE_PATH"
fi
if [[ ! -f "$ABS_FEATURE_PATH" ]]; then
  echo "Error: feature file not found: $FEATURE_PATH" >&2
  exit 1
fi

FEATURE_CONTENT="$(cat "$ABS_FEATURE_PATH")"

# Snapshot every existing tests/cli/cli_*_test.py file's content hash so we
# can tell afterwards which single file the model created or edited --
# necessary because the output path is no longer predetermined (the model
# may extend an existing file instead of creating a new one).
snapshot_tests_dir() {
  (cd "$REPO_ROOT/tests/cli" 2>/dev/null && \
    for f in cli_*_test.py; do \
      [[ -f "$f" ]] && echo "$f $(sha256sum "$f" | cut -d' ' -f1)"; \
    done)
  return 0
}
BEFORE_SNAPSHOT="$(snapshot_tests_dir | sort)"

if [[ -n "$TEST_PATH" ]]; then
  DESTINATION_INSTRUCTION="Save/append the result to exactly this path,
   relative to the repo root: ${TEST_PATH}
   (This exact path was explicitly requested by the person running this
   script -- use it even if a different existing file would otherwise be
   more pertinent.) If that file already exists, use \`edit\` to append a
   new test class (or new methods in a fitting existing class); if it does
   not exist yet, use \`create\`."
else
  DESTINATION_INSTRUCTION="Decide the single most pertinent destination
   file under tests/cli/ for this test, following the suite's existing
   per-command/per-feature file naming (e.g. a mount-related scenario
   belongs in a file like cli_mount_test.py or cli_mount_classic_test.py,
   never in a file named after an issue or PR number):
   - If a pertinent file already exists (check with glob/grep: tests/cli/cli_*_test.py),
     use \`edit\` to add a new test class (or new methods in an existing
     class covering the exact same sub-area) to that file, preserving
     everything already in it.
   - If no existing file fits, use \`create\` to write a new file whose
     name reflects the feature/command under test (e.g. cli_mount_test.py),
     NOT the issue/PR slug (never cli_issue_<N>_test.py / cli_pr_<N>_test.py
     unless that genuinely is the most descriptive name available).
   Touch exactly one tests/cli/*.py file this way -- never split the new
   scenarios across multiple files, and never touch any file other than
   that one (and never the feature file itself)."
fi

PROMPT=$(cat <<PROMPT_EOF
You are adding ONLY pytest scenario(s) to the Multipass CLI test suite
(tests/cli/), from an ALREADY HUMAN-APPROVED Gherkin feature below. Do this
and nothing else:

1. Read the approved Gherkin feature content below. It is the full and only
   source of truth for the behavior to test -- do not add assertions that
   aren't traceable to one of its Then/And steps, and do not invent
   additional scenarios.
2. Look at existing files under tests/cli/*.py and tests/cli/conftest.py
   with view/grep/glob to follow this suite's conventions: import helpers
   from cli.multipass / cli.utilities, use fixtures such as
   multipassd/instance, and mirror class/method naming patterns.
3. ${DESTINATION_INSTRUCTION}
4. Write one pytest method per Gherkin scenario. Add a short comment above
   each method linking it back to the scenario title it implements.
5. Directly above the test class you added (or, if appending methods to an
   existing class, directly above that class), include the FULL text of
   the approved Gherkin feature verbatim, wrapped in a single
   \`# Gherkin: ...\` comment block (one \`#\` per line, no blank line inside
   the block). This is the permanent, human-readable record of the spec
   those tests were generated from -- it must stay in sync with the
   .feature file content below, not be summarized or edited. If you are
   creating a brand-new file, this naturally lands right after the license
   header.
6. Do not run any command, and do not attempt to fetch or read any
   diff/code-change data for the originating issue/PR -- this feature file
   is the complete spec.

Approved Gherkin feature (${FEATURE_PATH}):
${FEATURE_CONTENT}
PROMPT_EOF
)

if [[ -n "$EXTRA_PROMPT" ]]; then
  PROMPT="${PROMPT}

Additional guidance from the person running this script (trusted, apply it):
${EXTRA_PROMPT}"
fi

echo "==> Invoking copilot to generate a test from ${FEATURE_PATH}" >&2
copilot --agent cli-test-generator \
  -p "$PROMPT" \
  --allow-all-tools \
  --available-tools view grep glob create edit \
  --no-ask-user \
  --silent \
  -C "$REPO_ROOT"

echo >&2

AFTER_SNAPSHOT="$(snapshot_tests_dir | sort)"
CHANGED_FILES="$(comm -13 <(echo "$BEFORE_SNAPSHOT") <(echo "$AFTER_SNAPSHOT") | awk '{print $1}')"
NUM_CHANGED="$(echo "$CHANGED_FILES" | grep -c . || true)"

if [[ -n "$TEST_PATH" ]]; then
  if [[ ! -f "$REPO_ROOT/$TEST_PATH" ]]; then
    echo "Error: expected output file was not created: $TEST_PATH" >&2
    exit 1
  fi
elif [[ "$NUM_CHANGED" -eq 0 ]]; then
  echo "Error: no tests/cli/cli_*_test.py file was created or modified." >&2
  exit 1
elif [[ "$NUM_CHANGED" -gt 1 ]]; then
  echo "Error: more than one file was touched, expected exactly one:" >&2
  echo "$CHANGED_FILES" | sed 's/^/  tests\/cli\//' >&2
  echo "Inspect these manually -- not auto-selecting one." >&2
  exit 1
else
  TEST_PATH="tests/cli/${CHANGED_FILES}"
fi

echo "==> Compile-checking ${TEST_PATH}" >&2
if python3 -m py_compile "$REPO_ROOT/$TEST_PATH"; then
  echo "    OK" >&2
else
  echo "    FAILED -- inspect ${TEST_PATH} before using it." >&2
fi

echo "==> Done. Review ${TEST_PATH}. Run it explicitly with:" >&2
echo "        pytest ${TEST_PATH} --daemon-controller=standalone --bin-dir=build/bin" >&2
