#!/bin/bash

# Script to fetch the human-written, behavior-only context for a GH issue
# and/or PR, for use by the CLI Behavior Test Generator agent
# (.github/agents/cli-test-generator.agent.md).
#
# Usage: ./fetch-context.sh (--issue N | --pr N) [--repo owner/repo]
# Options:
#   --issue N      Issue number to fetch.
#   --pr N         PR number to fetch. If the PR body references a "Fixes #N"
#                  / "Closes #N" issue, that issue is fetched too and included
#                  as `linked_issue`.
#   --repo OWNER/REPO  Target repo (default: canonical/multipass).
#   -h, --help     Show this help message.
#
# Deliberately NOT fetched: diff, changed files, commits, or any code content.
# This script only surfaces the human-written description of *intended*
# behavior (title/body/comments/labels), so a test generated from its output
# describes what the software should do, not what a particular patch did.

set -euo pipefail

REPO="canonical/multipass"
ISSUE_NUMBER=""
PR_NUMBER=""

usage() {
  sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --issue)
      ISSUE_NUMBER="$2"
      shift 2
      ;;
    --pr)
      PR_NUMBER="$2"
      shift 2
      ;;
    --repo)
      REPO="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -z "$ISSUE_NUMBER" && -z "$PR_NUMBER" ]]; then
  echo "Error: one of --issue or --pr is required." >&2
  usage >&2
  exit 1
fi

if ! command -v gh >/dev/null 2>&1; then
  echo "Error: 'gh' (GitHub CLI) is required but not found in PATH." >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "Error: 'jq' is required but not found in PATH." >&2
  exit 1
fi

# Fetch an issue's behavior-only fields: title, body, labels, comments.
# No diff/file data exists for an issue, so nothing needs to be stripped.
fetch_issue() {
  local number="$1"
  gh issue view "$number" --repo "$REPO" \
    --json number,title,body,labels,comments,url \
    2>/dev/null
}

# Fetch a PR's behavior-only fields. Explicitly request only the metadata
# fields below -- never diff/files/commits -- so the agent's context never
# includes the implementation itself.
fetch_pr() {
  local number="$1"
  gh pr view "$number" --repo "$REPO" \
    --json number,title,body,labels,comments,url \
    2>/dev/null
}

# Best-effort extraction of a "Fixes #N" / "Closes #N" style reference from a
# PR body, so the originating issue's description can be pulled in too.
extract_linked_issue() {
  local body="$1"
  grep -ioE '(fixes|closes|resolves)[[:space:]]*#[0-9]+' <<<"$body" \
    | grep -oE '[0-9]+' \
    | head -n 1 || true
}

ISSUE_JSON="null"
PR_JSON="null"
LINKED_ISSUE_JSON="null"

if [[ -n "$ISSUE_NUMBER" ]]; then
  if ! ISSUE_JSON=$(fetch_issue "$ISSUE_NUMBER"); then
    echo "Error: failed to fetch issue #$ISSUE_NUMBER from $REPO (check 'gh auth status')." >&2
    exit 1
  fi
fi

if [[ -n "$PR_NUMBER" ]]; then
  if ! PR_JSON=$(fetch_pr "$PR_NUMBER"); then
    echo "Error: failed to fetch PR #$PR_NUMBER from $REPO (check 'gh auth status')." >&2
    exit 1
  fi

  pr_body=$(jq -r '.body // ""' <<<"$PR_JSON")
  linked_number=$(extract_linked_issue "$pr_body")
  if [[ -n "$linked_number" ]]; then
    if linked_fetch=$(fetch_issue "$linked_number"); then
      LINKED_ISSUE_JSON="$linked_fetch"
    fi
  fi
fi

jq -n \
  --arg repo "$REPO" \
  --argjson issue "$ISSUE_JSON" \
  --argjson pr "$PR_JSON" \
  --argjson linked_issue "$LINKED_ISSUE_JSON" \
  '{
    repo: $repo,
    issue: $issue,
    pr: $pr,
    linked_issue: $linked_issue
  }'
