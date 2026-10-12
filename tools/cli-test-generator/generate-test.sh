#!/bin/bash

# Non-interactive alternative to chatting with the CLI Behavior Test
# Generator agent by hand: takes an already-approved Gherkin `.feature` file
# and invokes `copilot` once to add the matching pytest scenario(s) to the
# tests/cli suite -- either into an existing, pertinent file (e.g. adding a
# mount test to cli_mount_test.py) or, if none fits, a new, aptly-named file
# (e.g. cli_mount_classic_test.py). The output is never named after the
# issue/PR (no cli_issue_<N>_test.py) unless no better name applies.
# Nothing else happens after that -- no git, no cat, and no execution of
# the generated test itself (that needs sudo/a real VM backend and is left
# entirely to you). The script detects which file was touched,
# compile-checks it, collection-checks it (safe -- no daemon/build needed),
# prints manual red-before-green validation instructions, then reports its
# path and stops.
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
  sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'
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
   IMPORTANT framework rule: conftest.py's pytest_collection_modifyitems
   hook (maybe_skip_mount_test) runs for EVERY test item carrying the
   \`@pytest.mark.mount\` marker and unconditionally reads
   \`callspec.params["mount_type"]\`. If you apply \`@pytest.mark.mount\`
   to a class or function that is not also parametrized with
   \`@pytest.mark.parametrize("mount_type", ["classic", "native"])\`
   (directly, or inherited from an enclosing class), pytest collection
   crashes with \`AttributeError: 'NoneType' object has no attribute
   'params'\` for the WHOLE file, not just your new test. So: either
   parametrize every \`mount\`-marked test/class by \`mount_type\`, or do
   not apply the \`mount\` marker at all if the scenario doesn't need to
   vary by mount type.
3. ${DESTINATION_INSTRUCTION}
4. Write one pytest method per Gherkin scenario. Add a short comment above
   each method linking it back to the scenario title it implements, AND
   noting whether the scenario exercises the new/fixed behavior (must FAIL
   against the current, unfixed code) or verifies an unaffected/control
   case (e.g. "stays literal"/"unchanged" -- expected to pass either way).
   This distinction matters because a reviewer validating the file against
   the current build will see some scenarios genuinely pass before any fix
   exists -- tag which ones that's expected for, so a real false positive
   isn't mistaken for an intentional control case (or vice versa).
   IMPORTANT correctness rule: assert the EXACT, specific outcome the
   Gherkin describes, not a loose/weak check that unrelated pre-existing
   code could already satisfy by coincidence. Prefer exact equality (e.g.
   comparing a whole dict/string to its complete expected value) over
   membership/substring checks when the Gherkin implies an exact value.
   A test that passes even though the described fix/feature has not been
   implemented yet is a false positive and defeats the entire point of
   this pipeline -- if you are unsure whether an assertion would actually
   fail on unfixed code, make it stricter rather than looser.
   Additional rules for every scenario marked "must FAIL on unfixed code":
   - Hard-code the expected value as a literal in the test (e.g.
     "/home/ubuntu/target"); never derive it from the same input the test
     passes to the command, or from the command's own output.
   - Add a second, negative assertion that the exact buggy/unfixed value
     is absent (e.g. the literal unexpanded "~" component), so the test
     states both what must be true and what the bug looks like.
   - In the comment above the method, add a line starting with
     "Raw repro:" giving the bare multipass command(s) a human can type to
     see the bug by hand (e.g. \`multipass mount <src> <vm>:~/target\`
     then \`multipass info <vm> --format=json\`), so a surprising PASS can
     be cross-checked outside pytest.
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
COMPILE_OK=1
if python3 -m py_compile "$REPO_ROOT/$TEST_PATH"; then
  echo "    OK" >&2
else
  COMPILE_OK=0
  echo "    FAILED -- inspect ${TEST_PATH} before using it." >&2
fi

# py_compile only catches syntax errors. A second, cheap class of failure is
# collection-time breakage (e.g. a mismatched pytest marker/parametrize
# combination), which only surfaces when pytest actually imports the file
# and runs its collection hooks. --collect-only does this without needing a
# real daemon/build, so run it scoped to just this one file.
if [[ "$COMPILE_OK" -eq 1 ]] && command -v pytest >/dev/null 2>&1; then
  echo "==> Collection-checking ${TEST_PATH} (pytest --collect-only)" >&2
  if (cd "$REPO_ROOT" && python3 -m pytest "${TEST_PATH}" --collect-only -q) >/tmp/cli-test-gen-collect.$$ 2>&1; then
    echo "    OK" >&2
  else
    echo "    FAILED -- pytest could not collect ${TEST_PATH}:" >&2
    sed 's/^/    /' /tmp/cli-test-gen-collect.$$ >&2
    echo "    If this mentions 'mount_type'/callspec, the test applies" >&2
    echo "    @pytest.mark.mount without a matching" >&2
    echo "    @pytest.mark.parametrize(\"mount_type\", [...]) -- fix one of" >&2
    echo "    those two before using this file." >&2
  fi
  rm -f /tmp/cli-test-gen-collect.$$
else
  echo "==> Skipping collection-check (pytest not found in PATH)." >&2
fi

# NOTE: this script deliberately does NOT execute the generated test
# against a real daemon/build -- doing so needs sudo, a real VM backend
# (KVM etc.), and can conflict with or be masked by an already-running
# system multipass install (snap/launchd/service shares the same default
# bridge interface names/subnets), which breaks unpredictably in
# environments that aren't set up for it. Validating red-before-green (did
# this test actually fail before any fix/feature exists?) is therefore a
# manual step for you to run yourself, never automated here:
echo "==> Not running the test for real (needs sudo/a VM backend/a real" >&2
echo "    daemon -- left entirely to you). Before trusting this test as a" >&2
echo "    regression/acceptance check, confirm it actually FAILS against" >&2
echo "    the current (pre-fix) build, scenario by scenario:" >&2
echo "        pytest ${TEST_PATH} --daemon-controller=standalone --bin-dir=<path-to-build/bin> -rA -v" >&2
echo "    - Never trust a bare 'pytest ${TEST_PATH}' with no" >&2
echo "      --daemon-controller/--bin-dir flags for this -- it silently" >&2
echo "      falls back to the system-installed snap multipass, not this" >&2
echo "      checkout's own code." >&2
echo "    - Judge each scenario individually, not the aggregate exit code:" >&2
echo "      a file commonly mixes scenarios that must fail pre-fix with" >&2
echo "      ones intentionally verifying unaffected/control behavior" >&2
echo "      (expected to pass either way). If ANY scenario passes that" >&2
echo "      exercises the new/fixed behavior, that's a likely false" >&2
echo "      positive -- tighten its assertions (exact equality over" >&2
echo "      membership/substring checks) and re-confirm by" >&2
echo "      stashing/reverting any local fix and re-running." >&2
echo "    - If a system multipass service is already running" >&2
echo "      (snap/launchd/service), stop it first -- it can conflict with" >&2
echo "      (and crash on startup) the standalone daemon via shared bridge" >&2
echo "      interfaces/subnets, or leave stray dnsmasq processes bound to" >&2
echo "      those names." >&2
echo "    - Before concluding a surprising PASS means the assertion is too" >&2
echo "      loose, cross-check independently with a raw, non-pytest" >&2
echo "      'multipass' command against the exact same binary (e.g." >&2
echo "      'multipass mount <src> <instance>:<target>' then 'multipass" >&2
echo "      info <instance> --format=json', read the mounts key by eye)." >&2
echo "      If that raw check shows the behavior is genuinely unfixed while" >&2
echo "      pytest reported PASS for the same scenario, pytest likely did" >&2
echo "      not exercise what you think -- rule out: --bin-dir/PATH" >&2
echo "      resolving to a different multipass than you just built; a" >&2
echo "      stale __pycache__ serving an older/looser version of a" >&2
echo "      hand-edited test file (clear it, re-run with --cache-clear);" >&2
echo "      or leftover persisted daemon state from reusing the same" >&2
echo "      --storage-dir across runs (use a fresh one, or omit the flag)." >&2

echo "==> Done. Review ${TEST_PATH} before deciding whether to keep it." >&2
