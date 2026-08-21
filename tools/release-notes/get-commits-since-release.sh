#!/bin/bash

# Script to fetch all commits since the last Multipass release
# Usage: ./get-commits-since-release.sh [options]
# Options:
#   -f, --format FORMAT   Output format: oneline, short, medium, full (default: oneline)
#   -s, --stat            Show diffstat
#   -a, --author          Group by author
#   --tag TAG             Use specific release tag (default: auto-detect latest)
#   -h, --help            Show this help message

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions for JSON generation
extract_pr_number() {
  local subject="$1"
  # Extract PR number from format like "(#5078)"
  if [[ $subject =~ \(#([0-9]+)\) ]]; then
    echo "${BASH_REMATCH[1]}"
  else
    echo ""
  fi
}

extract_category() {
  local subject="$1"
  # Extract category from format like "[qemu]" at the start
  if [[ $subject =~ ^\[([a-zA-Z0-9-]+)\] ]]; then
    echo "${BASH_REMATCH[1]}"
  else
    echo "other"
  fi
}

detect_commit_type() {
  local subject="$1"
  local body="$2"
  local combined="$subject"$'\n'"$body"
  
  # Check for breaking changes
  if echo "$combined" | grep -iqE 'BREAKING|breaking change|remove|deprecat'; then
    echo "breaking"
    return
  fi
  
  # Check for feature/new
  if echo "$combined" | grep -iqE '\[feature\]|add|implement|new'; then
    echo "feature"
    return
  fi
  
  # Check for fixes
  if echo "$combined" | grep -iqE '\[fix\]|\[bug\]|fix|resolve|resolves'; then
    echo "fix"
    return
  fi
  
  # Check for docs
  if echo "$combined" | grep -iqE '\[doc\]|\[docs\]|documentation'; then
    echo "docs"
    return
  fi
  
  # Check for performance
  if echo "$combined" | grep -iqE 'performance|optimize|perf'; then
    echo "performance"
    return
  fi
  
  echo "other"
}

# Decide whether a commit is release-notes noise that can be dropped BEFORE
# enrichment (so we skip the gh fetch and keep it out of subagent context).
# Uses only cheap fields: category, author, subject. Echoes a skip reason, or
# nothing if the commit should be kept.
classify_skip() {
  local category="$1"
  local author="$2"
  local subject="$3"

  # Automation authors are pure churn (dependency/CI bots).
  # copilot-swe-agent is a real contributor and must NOT be skipped by author.
  case "$author" in
    "copilot-swe-agent[bot]") ;;
    renovate\[bot\]|dependabot\[bot\]|multipass-ci\[bot\]|github-actions\[bot\])
      echo "automation-author"
      return
      ;;
  esac

  # CI / repository-plumbing categories that never reach end users.
  case "$category" in
    ci|ci-clang-tidy|clang-tidy|tics|format|commit-msg-hook|git|github|submodules|tests)
      echo "ci-infra"
      return
      ;;
  esac

  # Nothing matched: keep it (deps/cmake/build/docs are demoted downstream,
  # not skipped, because human-authored ones can be notable).
  echo ""
}

# Default values
FORMAT="oneline"
GROUP_BY_AUTHOR=false
NEW_AUTHORS_ONLY=false
MERGE_ONLY=false
JSON_OUTPUT=false
ENRICH=false
CUSTOM_TAG=""
HELP=false

# Repository used to resolve PR metadata (override with PR_REPO env var)
PR_REPO="${PR_REPO:-canonical/multipass}"

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -f|--format)
      FORMAT="$2"
      shift 2
      ;;
    -a|--author)
      GROUP_BY_AUTHOR=true
      shift
      ;;
    --new-authors)
      GROUP_BY_AUTHOR=true
      NEW_AUTHORS_ONLY=true
      shift
      ;;
    -m|--merge)
      MERGE_ONLY=true
      shift
      ;;
    --json)
      JSON_OUTPUT=true
      shift
      ;;
    --enrich)
      ENRICH=true
      shift
      ;;
    --tag)
      CUSTOM_TAG="$2"
      shift 2
      ;;
    -h|--help)
      HELP=true
      shift
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Show help
if [[ $HELP == true ]]; then
  echo "Usage: $(basename "$0") [options]"
  echo ""
  echo "Fetch all commits since the last Multipass release."
  echo ""
  echo "Options:"
  echo "  -f, --format FORMAT   Output format: oneline, short, medium, full (default: oneline)"
  echo "  -a, --author          Group by author"
  echo "  --new-authors         Show only new authors (first-time contributors)"
  echo "  -m, --merge           Show only merge commits from PRs"
  echo "  --json                Output as JSON with metadata and categorization"
  echo "  --enrich              With --json, fetch PR title/body/labels/diffstat via gh"
  echo "  --tag TAG             Use specific release tag (default: auto-detect latest)"
  echo "  -h, --help            Show this help message"
  echo ""
  echo "Examples:"
  echo "  $(basename "$0")                      # Show commits since last release"
  echo "  $(basename "$0") -f medium            # Show with more detail"
  echo "  $(basename "$0") -a                   # Group by author"
  echo "  $(basename "$0") --new-authors        # Show only new authors since release"
  echo "  $(basename "$0") -m                   # Show only merge commits"
  echo "  $(basename "$0") --tag v1.16.0        # Use specific tag"
  exit 0
fi

# Determine the repository root
if ! git rev-parse --git-dir > /dev/null 2>&1; then
  echo -e "${RED}Error: Not in a git repository${NC}"
  exit 1
fi

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT" || exit 1

# Find the latest release tag
if [[ -z $CUSTOM_TAG ]]; then
  # Look for release tags matching v<digit>.<digit>.<digit> (excluding -dev, -rc, etc.)
  LATEST_TAG=$(git tag -l 'v[0-9]*' --sort=-version:refname | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | head -1)
  
  if [[ -z $LATEST_TAG ]]; then
    # Fallback: get any v-prefixed tag
    LATEST_TAG=$(git tag -l 'v[0-9]*' --sort=-version:refname | head -1)
  fi
  
  if [[ -z $LATEST_TAG ]]; then
    echo -e "${RED}Error: No release tags found${NC}"
    exit 1
  fi
else
  LATEST_TAG=$CUSTOM_TAG
fi

# Verify the tag exists
if ! git rev-parse "$LATEST_TAG" > /dev/null 2>&1; then
  echo -e "${RED}Error: Tag '$LATEST_TAG' not found${NC}"
  exit 1
fi

# Get commit count and summary
COMMIT_COUNT=$(git rev-list --count "$LATEST_TAG..HEAD")

if [[ $JSON_OUTPUT == false ]]; then
  echo -e "${BLUE}Commits since ${GREEN}${LATEST_TAG}${BLUE}:${NC} ${GREEN}${COMMIT_COUNT}${NC}"
  echo ""
fi

# Build git log command
MERGE_FLAG=""
if [[ $MERGE_ONLY == true ]]; then
  MERGE_FLAG="--merges"
fi

case $FORMAT in
  oneline)
    LOG_CMD="git log $LATEST_TAG..HEAD $MERGE_FLAG --oneline"
    ;;
  short)
    LOG_CMD="git log $LATEST_TAG..HEAD $MERGE_FLAG --format='%h%n%an%n%s%n'"
    ;;
  medium)
    LOG_CMD="git log $LATEST_TAG..HEAD $MERGE_FLAG --format='%H%n%an <%ae>%naD%n%s%n%b%n'"
    ;;
  full)
    LOG_CMD="git log $LATEST_TAG..HEAD $MERGE_FLAG"
    ;;
  *)
    echo -e "${RED}Error: Unknown format '$FORMAT'${NC}"
    exit 1
    ;;
esac

# Output the log
if [[ $GROUP_BY_AUTHOR == true ]]; then
  # Show summary by author
  if [[ $NEW_AUTHORS_ONLY == true ]]; then
    echo -e "${YELLOW}New authors since ${LATEST_TAG}:${NC}"
    # Get all authors before the release tag
    HISTORICAL_AUTHORS=$(git log --all --before="$(git log -1 --format=%aI $LATEST_TAG)" --format=format:"%aN" | sort | uniq)
    # Get all authors since the release tag
    NEW_RELEASE_AUTHORS=$(git log "$LATEST_TAG..HEAD" --format=format:"%aN")
    # Find authors that are new (not in historical authors)
    echo "$NEW_RELEASE_AUTHORS" | sort | uniq | while read author; do
      if ! echo "$HISTORICAL_AUTHORS" | grep -Fxq "$author"; then
        COMMIT_COUNT=$(echo "$NEW_RELEASE_AUTHORS" | grep -c "^$author$")
        echo "  $COMMIT_COUNT commits - $author"
      fi
    done | sort -rn
  else
    echo -e "${YELLOW}Commits by author:${NC}"
    git log "$LATEST_TAG..HEAD" --format=format:"%aN" | sort | uniq -c | sort -rn | while read count author; do
      echo "  $count commits - $author"
    done
  fi
elif [[ $JSON_OUTPUT == true ]]; then
  # Generate JSON output with categorization
  HISTORICAL_AUTHORS=$(git log --all --before="$(git log -1 --format=%aI $LATEST_TAG)" --format=format:"%aN" 2>/dev/null | sort | uniq)

  # Enrichment requires the GitHub CLI
  if [[ $ENRICH == true ]] && ! command -v gh > /dev/null 2>&1; then
    echo -e "${YELLOW}Warning: --enrich requested but 'gh' not found; continuing without PR metadata${NC}" >&2
    ENRICH=false
  fi

  # Per-PR cache so re-runs don't re-hit the GitHub API
  PR_CACHE_DIR="${TMPDIR:-/tmp}/mp-pr-cache"
  mkdir -p "$PR_CACHE_DIR"

  # Track enrichment outcomes so silent gh failures (auth, rate limits, wrong
  # PR_REPO) can be surfaced as a warning instead of producing thin records.
  ENRICH_ATTEMPTED=0
  ENRICH_FAILED=0

  # One JSON object per line, slurped into an array at the end
  RECORDS_FILE=$(mktemp)
  trap "rm -f $RECORDS_FILE" EXIT

  while IFS=$'\n' read -r line; do
    [[ -z $line ]] && continue

    hash=$(echo "$line" | cut -d'|' -f1)
    author=$(echo "$line" | cut -d'|' -f2)
    subject=$(echo "$line" | cut -d'|' -f3-)

    # Apply merge filter
    if [[ $MERGE_ONLY == true ]]; then
      IS_MERGE=$(git log -1 "$hash" --format=%b | grep -c "^Merge" || echo 0)
      if [[ $IS_MERGE -eq 0 ]]; then
        continue
      fi
    fi

    pr_number=$(extract_pr_number "$subject")
    category=$(extract_category "$subject")

    is_new_author=false
    if ! echo "$HISTORICAL_AUTHORS" | grep -Fxq "$author"; then
      is_new_author=true
    fi

    # Early-exit classification: skip release-notes noise before enrichment.
    skip_reason=$(classify_skip "$category" "$author" "$subject")
    skip=false
    [[ -n $skip_reason ]] && skip=true

    # Fetch PR metadata (title, body, labels, diffstat, files) when enriching.
    # Skipped commits are never enriched: no gh call, no body in context.
    enrich="{}"
    if [[ $ENRICH == true && $skip == false && -n $pr_number ]]; then
      cache_file="$PR_CACHE_DIR/${PR_REPO//\//_}-${pr_number}.json"
      if [[ ! -s $cache_file ]]; then
        gh pr view "$pr_number" --repo "$PR_REPO" \
          --json title,body,labels,additions,deletions,changedFiles,files \
          > "$cache_file" 2>/dev/null || echo '{}' > "$cache_file"
      fi
      enrich=$(cat "$cache_file")
      [[ -z $enrich ]] && enrich="{}"

      # Count a failure when the fetch produced no usable title.
      ENRICH_ATTEMPTED=$((ENRICH_ATTEMPTED + 1))
      if [[ $(echo "$enrich" | jq -r '.title // ""' 2>/dev/null) == "" ]]; then
        ENRICH_FAILED=$((ENRICH_FAILED + 1))
      fi
    fi

    # Classify using the PR body when available, not just the subject line
    body_for_type=""
    if [[ $enrich != "{}" ]]; then
      body_for_type=$(echo "$enrich" | jq -r '.body // ""' 2>/dev/null || echo "")
    fi
    commit_type=$(detect_commit_type "$subject" "$body_for_type")

    # Build the record; merge enrichment fields when present
    jq -nc \
      --arg hash "$hash" \
      --arg author "$author" \
      --arg subject "$subject" \
      --arg category "$category" \
      --arg type "$commit_type" \
      --argjson pr "${pr_number:-null}" \
      --argjson new "$([[ $is_new_author == true ]] && echo true || echo false)" \
      --argjson skip "$skip" \
      --arg skip_reason "$skip_reason" \
      --argjson enrich "$enrich" \
      '{
        hash: $hash,
        author: $author,
        subject: $subject,
        category: $category,
        type: $type,
        pr_number: $pr,
        is_new_author: $new,
        skip: $skip,
        skip_reason: (if $skip_reason == "" then null else $skip_reason end)
      }
      + (if ($enrich | type) == "object" and ($enrich | keys | length) > 0 then {
          pr_title: ($enrich.title // null),
          pr_body: (($enrich.body // "") | .[0:4000]),
          labels: [ (($enrich.labels // [])[] | .name) ],
          additions: ($enrich.additions // null),
          deletions: ($enrich.deletions // null),
          changed_files: ($enrich.changedFiles // null),
          top_dirs: (
            [ (($enrich.files // [])[] | .path | split("/") | .[0:2] | join("/")) ]
            | group_by(.) | map({dir: .[0], count: length})
            | sort_by(-.count) | .[0:6]
          )
        } else {} end)' >> "$RECORDS_FILE"
  done < <(git log "$LATEST_TAG..HEAD" --format="%h|%aN|%s")

  # Surface enrichment problems on stderr (stdout stays valid JSON).
  if [[ $ENRICH == true && $ENRICH_ATTEMPTED -gt 0 ]]; then
    if [[ $ENRICH_FAILED -eq $ENRICH_ATTEMPTED ]]; then
      echo -e "${RED}Error: enrichment failed for all ${ENRICH_ATTEMPTED} PRs. Check 'gh auth status' and that PR_REPO='${PR_REPO}' is correct.${NC}" >&2
    elif [[ $ENRICH_FAILED -gt 0 ]]; then
      echo -e "${YELLOW}Warning: enrichment failed for ${ENRICH_FAILED}/${ENRICH_ATTEMPTED} PRs (records fall back to subject line). Possible rate limiting; re-run to retry via cache.${NC}" >&2
    fi
  fi

  jq -n \
    --arg tag "$LATEST_TAG" \
    --argjson count "$COMMIT_COUNT" \
    --arg gen "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    --slurpfile commits "$RECORDS_FILE" \
    '{
      metadata: {
        release_tag: $tag,
        total_commits: $count,
        generated_at: $gen
      },
      commits: $commits
    }'

else
  # Execute the log command for human-readable output
  eval "$LOG_CMD"
fi
