#!/bin/bash

# Convenience wrapper: runs the CLI Behavior Test Generator agent
# (.github/agents/cli-test-generator.agent.md) in "Generate test from
# Gherkin" mode for an already-approved `.feature` file. All the actual
# instructions live in the agent file; this script only builds a short
# request, launches `copilot` with a restricted tool list (no bash/git/gh),
# detects which single tests/cli/cli_*_test.py file was created or edited,
# and runs the safe checks (py_compile and pytest --collect-only). It never
# runs the generated test against a real daemon -- that is left to you.
#
# Usage: ./generate-test.sh --feature path/to/file.feature [--prompt "extra guidance"] [--test-path PATH]
# Options:
#   --feature PATH    Path to the approved .feature file (required), e.g.
#                     tests/cli/issue_1234.feature
#   --prompt TEXT     Extra free-text guidance for the test implementation,
#                     e.g. "use the `instance` fixture". Optional.
#   --test-path PATH  Force the output test file path (default: the agent
#                     picks a pertinent existing file or a new aptly-named one).
#   --check-only      Do not invoke copilot: only run the compile and
#                     collection checks and print the validation command for
#                     the existing file given with --test-path (--feature is
#                     not needed). Use it after generating in an interactive
#                     agent session (e.g. VS Code), which has no shell access.
#                     No pre-generation copy exists in this mode, so the printed
#                     command selects every test class in the file; narrow it
#                     to the tests that were added before running it.
#   -h, --help        Show this help message.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

FEATURE_PATH=""
EXTRA_PROMPT=""
TEST_PATH=""
CHECK_ONLY=0

usage() {
  sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --feature) FEATURE_PATH="$2"; shift 2 ;;
    --prompt) EXTRA_PROMPT="$2"; shift 2 ;;
    --test-path) TEST_PATH="$2"; shift 2 ;;
    --check-only) CHECK_ONLY=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

if [[ "$CHECK_ONLY" -eq 1 && -z "$TEST_PATH" ]]; then
  echo "Error: --check-only requires --test-path." >&2
  exit 1
fi

if [[ "$CHECK_ONLY" -eq 0 && -z "$FEATURE_PATH" ]]; then
  echo "Error: --feature is required." >&2
  usage >&2
  exit 1
fi

if [[ "$CHECK_ONLY" -eq 0 ]] && ! command -v copilot >/dev/null 2>&1; then
  echo "Error: 'copilot' (GitHub Copilot CLI) is required but not found in PATH." >&2
  exit 1
fi

ABS_TEST_PATH=""
if [[ -n "$TEST_PATH" ]]; then
  ABS_TEST_PATH="$REPO_ROOT/$TEST_PATH"
  [[ "$TEST_PATH" = /* ]] && ABS_TEST_PATH="$TEST_PATH"
  ABS_TEST_PATH="$(python3 -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' "$ABS_TEST_PATH")"
  TESTS_DIR="$(python3 -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' "$REPO_ROOT/tests/cli")"
  if [[ "$(dirname "$ABS_TEST_PATH")" != "$TESTS_DIR" || ! "$(basename "$ABS_TEST_PATH")" =~ ^cli_.+_test\.py$ ]]; then
    echo "Error: --test-path must be tests/cli/cli_*_test.py inside the repository." >&2
    exit 1
  fi
  TEST_PATH="tests/cli/$(basename "$ABS_TEST_PATH")"
fi

ABS_FEATURE_PATH="$REPO_ROOT/$FEATURE_PATH"
if [[ "$FEATURE_PATH" = /* ]]; then
  ABS_FEATURE_PATH="$FEATURE_PATH"
fi
if [[ -n "$FEATURE_PATH" && ! -f "$ABS_FEATURE_PATH" ]]; then
  echo "Error: feature file not found: $FEATURE_PATH" >&2
  exit 1
fi

# Snapshot content hashes of every tests/cli/cli_*_test.py so we can tell
# afterwards which single file the agent created or edited.
snapshot_tests_dir() {
  (cd "$REPO_ROOT/tests/cli" 2>/dev/null && \
    for f in cli_*_test.py; do \
      [[ -f "$f" ]] && echo "$f $(if command -v sha256sum >/dev/null 2>&1; then sha256sum "$f"; else shasum -a 256 "$f"; fi | cut -d' ' -f1)"; \
    done)
  return 0
}
BEFORE_SNAPSHOT="$(snapshot_tests_dir | sort)"

# Keep a copy of the files as they were, to work out afterwards which test
# classes/methods the agent added (so only those are run, not the whole file).
BACKUP_DIR="$(mktemp -d)"
trap 'rm -rf "$BACKUP_DIR"' EXIT
for f in "$REPO_ROOT"/tests/cli/cli_*_test.py; do
  [[ -f "$f" ]] && cp "$f" "$BACKUP_DIR/"
done
if [[ "$CHECK_ONLY" -eq 0 && -n "$TEST_PATH" && -f "$ABS_TEST_PATH" ]]; then
  cp "$ABS_TEST_PATH" "$BACKUP_DIR/"
fi

if [[ "$CHECK_ONLY" -eq 1 ]]; then
  if [[ ! -f "$ABS_TEST_PATH" ]]; then
    echo "Error: test file not found: $TEST_PATH" >&2
    exit 1
  fi
else

PROMPT="Mode 2 — Generate test from Gherkin, as defined in your agent instructions.
The approved feature file is ${FEATURE_PATH}; read it with the view tool.
This is a headless run with no command execution: skip steps 6 and 7 (the
calling script runs the compile/collection checks and prints the validation
instructions) and do not run any command."

if [[ -n "$TEST_PATH" ]]; then
  PROMPT="${PROMPT}
Destination: ${TEST_PATH} (explicitly requested; use it even if another file
would be more pertinent)."
fi

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
  if [[ "$NUM_CHANGED" -gt 0 && ( "$NUM_CHANGED" -ne 1 || "$CHANGED_FILES" != "$(basename "$TEST_PATH")" ) ]]; then
    echo "Error: generation modified unexpected test files; expected only ${TEST_PATH}:" >&2
    echo "$CHANGED_FILES" | sed 's/^/  tests\/cli\//' >&2
    exit 1
  fi
  if [[ ! -f "$ABS_TEST_PATH" ]]; then
    echo "Error: expected output file was not created: $TEST_PATH" >&2
    exit 1
  fi
  if [[ -f "$BACKUP_DIR/$(basename "$TEST_PATH")" ]] &&
     cmp -s "$BACKUP_DIR/$(basename "$TEST_PATH")" "$ABS_TEST_PATH"; then
    echo "Error: ${TEST_PATH} was not modified by the generation." >&2
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
  ABS_TEST_PATH="$REPO_ROOT/$TEST_PATH"
fi

fi # CHECK_ONLY

echo "==> Compile-checking ${TEST_PATH}" >&2
COMPILE_OK=1
if python3 -m py_compile "$ABS_TEST_PATH"; then
  echo "    OK" >&2
else
  COMPILE_OK=0
  echo "    FAILED -- inspect ${TEST_PATH} before using it." >&2
  exit 1
fi

# py_compile only catches syntax errors; --collect-only also catches
# collection-time breakage (e.g. @pytest.mark.mount without a mount_type
# parametrize) and needs no daemon/build.
if [[ "$COMPILE_OK" -eq 1 ]] && command -v pytest >/dev/null 2>&1; then
  echo "==> Collection-checking ${TEST_PATH} (pytest --collect-only)" >&2
  COLLECT_LOG="$(mktemp)"
  if (cd "$REPO_ROOT" && python3 -m pytest "${TEST_PATH}" --collect-only -q) >"$COLLECT_LOG" 2>&1; then
    echo "    OK" >&2
  else
    echo "    FAILED -- pytest could not collect ${TEST_PATH}:" >&2
    sed 's/^/    /' "$COLLECT_LOG" >&2
    rm -f "$COLLECT_LOG"
    exit 1
  fi
else
  echo "==> Skipping collection-check (pytest not found or compile failed)." >&2
fi

# Select only the added tests: classes added in the diff (or the whole file's
# classes if it is new); if the agent only appended methods to an existing
# class, select those methods by name with -k instead.
ADDED_LINES="$(diff "$BACKUP_DIR/$(basename "$TEST_PATH")" "$ABS_TEST_PATH" 2>/dev/null \
  | sed -n 's/^> //p' || true)"
if [[ ! -f "$BACKUP_DIR/$(basename "$TEST_PATH")" ]]; then
  ADDED_LINES="$(cat "$ABS_TEST_PATH")"
fi
ADDED_CLASSES="$(sed -n 's/^class \([A-Za-z0-9_]*\).*/\1/p' <<<"$ADDED_LINES")"
ADDED_METHODS="$(sed -n 's/^ *def \(test_[A-Za-z0-9_]*\).*/\1/p' <<<"$ADDED_LINES")"
if [[ -n "$ADDED_CLASSES" ]]; then
  SELECTOR=""
  while read -r cls; do SELECTOR+=" ${TEST_PATH}::${cls}"; done <<<"$ADDED_CLASSES"
elif [[ -n "$ADDED_METHODS" ]]; then
  SELECTOR=" ${TEST_PATH} -k \"$(paste -sd' ' <<<"$ADDED_METHODS" | sed 's/ / or /g')\""
else
  SELECTOR=" ${TEST_PATH}"
  echo "==> Could not tell which tests were added; the command below runs the whole file." >&2
fi

if [[ "$CHECK_ONLY" -eq 1 ]]; then
  echo "==> --check-only cannot tell which tests were added: the command below selects every class in the file; narrow it to the new tests." >&2
fi

echo "==> Not running the test for real (needs sudo/a VM backend). Validate" >&2
echo "    red-before-green yourself against a build of the UNFIXED code" >&2
echo "    (this runs only the tests that were added):" >&2
echo "        pytest${SELECTOR} --daemon-controller=standalone --bin-dir=<path-to-build/bin> -rA -v" >&2
echo "    Judge each scenario individually; a bare pytest without those" >&2
echo "    flags tests the system snap. See the agent file for details." >&2

echo "==> Done. Review ${TEST_PATH} before deciding whether to keep it." >&2
