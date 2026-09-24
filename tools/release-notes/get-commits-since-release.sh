#!/bin/bash

# Script to fetch all commits since the last Multipass release
# Usage: ./get-commits-since-release.sh [options]
# Options:
#   -f, --format FORMAT   Output format: oneline, short, medium, full (default: oneline)
#   -a, --author          Group by author
#   --tag TAG             Start of range: previous release tag (default: auto-detect latest)
#   --to REF              End of range: target ref (default: HEAD)
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
  local parenthesized
  # Extract PR number from squash-merge format like "(#5078)", or from
  # merge-commit format like "Merge pull request #5078 from ..." (used heavily
  # before squash merging became the norm, e.g. the v1.15..v1.16 era).
  # Backport subjects can contain both the original PR and the backport PR,
  # e.g. "... (#4198) (#4352)". The final parenthesized number is the PR that
  # landed the commit in the release branch and is the one whose author counts.
  if [[ $subject =~ ^Merge\ pull\ request\ \#([0-9]+) ]]; then
    echo "${BASH_REMATCH[1]}"
  elif parenthesized=$(grep -oE '\(#[0-9]+\)' <<<"$subject" | tail -n 1); then
    grep -oE '[0-9]+' <<<"$parenthesized"
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

  # Nothing matched: keep it (deps/cmake/build/docs are evaluated downstream,
  # not skipped, because human-authored ones can be user-facing, e.g. runtime
  # upgrades or installability fixes).
  echo ""
}

# Populate SHIPPED_PR with every PR number that already shipped in a published
# release before the target, computed on the fly from local git. No persistent
# ledger is kept: a PR "shipped" iff its number appears in a commit reachable
# from a prior published release tag, which holds across divergent feature and
# maintenance branches (a cherry-pick carries the PR number in its subject on
# the release branch).
#
# Published releases in this repo are the signed vMAJOR.MINOR.PATCH tags; RC and
# dev builds carry -rc/-dev suffixes and are excluded. For historical
# regeneration where the target is itself a release tag, only lower-versioned
# releases count as prior.
compute_prior_shipped_prs() {
  local -a tags=() prior=() refs=()
  local tag
  while IFS= read -r tag; do
    [[ -n $tag ]] && tags+=("$tag")
  done < <(git tag -l 'v[0-9]*' | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V)

  if printf '%s\n' "${tags[@]}" | grep -qxF "$TO_REF"; then
    for tag in "${tags[@]}"; do
      [[ $tag == "$TO_REF" ]] && break
      prior+=("$tag")
    done
  else
    prior=("${tags[@]}")
  fi

  # Keep only tags that resolve locally (a shallow clone may miss some).
  for tag in "${prior[@]}"; do
    git rev-parse --verify --quiet "${tag}^{commit}" >/dev/null 2>&1 && refs+=("$tag")
  done
  PRIOR_RELEASE_COUNT=${#refs[@]}
  [[ ${#refs[@]} -eq 0 ]] && return 0

  # Every PR number referenced by any commit reachable from a prior release.
  # Capturing every "(#N)" (not just the last one) is deliberate: a backport
  # subject like "... (#4198) (#4352)" marks BOTH numbers as shipped.
  local n
  while IFS= read -r n; do
    [[ -n $n ]] && SHIPPED_PR["$n"]=1
  done < <(git log "${refs[@]}" --format='%s' 2>/dev/null \
            | grep -oE '\(#[0-9]+\)|^Merge pull request #[0-9]+' \
            | grep -oE '[0-9]+')
  return 0
}

# Classify a PR author login as a first-time contributor for this release,
# defined as "first shipped in a published release". Echoes true, false, or
# unresolved. Memoized per login.
#
# 1. git-only shortcut: if any of the author's in-range PRs already shipped in a
#    prior release (e.g. a maintenance cherry-pick), they are not new.
# 2. otherwise, one GitHub query for the author's merged PR numbers; if any
#    already shipped in a prior release, they are not new; else they are new.
classify_new_login() {
  local login="$1"
  [[ -n ${LOGIN_STATUS[$login]:-} ]] && { echo "${LOGIN_STATUS[$login]}"; return; }

  local pr
  for pr in ${AUTHOR_INRANGE_PRS[$login]:-}; do
    if [[ -n ${SHIPPED_PR[$pr]:-} ]]; then
      LOGIN_STATUS[$login]=false; echo false; return
    fi
  done

  if ! command -v gh >/dev/null 2>&1; then
    LOGIN_STATUS[$login]=unresolved; echo unresolved; return
  fi

  local nums
  if ! nums=$(gh search prs --repo "$PR_REPO" --author "$login" --merged \
      --limit 1000 --json number --jq '.[].number' 2>/dev/null); then
    echo "Warning: could not query merged PR history for GitHub login '$login'; contributor status is unresolved" >&2
    LOGIN_STATUS[$login]=unresolved; echo unresolved; return
  fi

  local n
  for n in $nums; do
    if [[ -n ${SHIPPED_PR[$n]:-} ]]; then
      LOGIN_STATUS[$login]=false; echo false; return
    fi
  done
  LOGIN_STATUS[$login]=true; echo true
}

# --- PR enrichment cache helpers -------------------------------------------
#
# Two failure modes motivated these helpers, both of which caused "enrichment
# failed for all PRs" even with valid auth:
#   1. A bare "{}" used to be written to the cache on ANY fetch failure. Because
#      "{}" is non-empty, the "[[ ! -s ]]" guard treated it as a permanent hit,
#      so a transient error (auth blip, throttling) poisoned the cache and
#      blocked every future retry.
#   2. One "gh pr view" (a GraphQL call) per PR, fired in a tight loop, trips
#      GitHub's SECONDARY rate limit after ~15 calls. Those rejections were then
#      cached as "{}" (see 1), compounding into a total failure.
# The fixes: never persist a failure; distinguish a permanent "not a PR" result
# (cache a sentinel, never retry) from transient throttling (retry with
# backoff, never cache); and batch-fetch via GraphQL aliases so we make ~a
# dozen requests instead of hundreds.

# Cached when GitHub resolves a number to "not a pull request" (e.g. an issue
# or a cross-repo reference). Distinct from a transient failure: it IS cached
# (so we never retry it) but is never counted as an enrichment failure.
PR_NOTFOUND_SENTINEL='{"__notfound__":true}'

# jq program reshaping a GraphQL pullRequest node into the same shape the rest
# of the script expects from `gh pr view --json title,body,labels,...`.
GQL_TO_PRVIEW='{title, body, author_login: (.author.login // null), author_type: (.author.__typename // null), labels: [ .labels.nodes[]? | {name} ], additions, deletions, changedFiles, files: [ .files.nodes[]? | {path} ]}'

# Fetch a single PR, distinguishing permanent from transient failures. On
# success (or a confirmed "not a PR") it writes the cache; on a transient error
# it retries with exponential backoff and, if still failing, writes NOTHING so a
# later run can retry — it never poisons the cache with "{}". Returns non-zero
# only on an unresolved transient failure. Used as a per-PR fallback for
# anything the batch pre-pass didn't populate.
fetch_one_pr() {
  local pr="$1" cache_file="$2"
  local attempt=0 max_attempts=4 out err_file
  err_file=$(mktemp)
  while :; do
    if out=$(gh pr view "$pr" --repo "$PR_REPO" \
      --json title,body,author,labels,additions,deletions,changedFiles,files 2>"$err_file"); then
      printf '%s' "$out" | jq \
        '.["author_login"] = (.author.login // null)
         | .["author_type"] = (if .author.is_bot == true then "Bot" else "User" end)
         | del(.author)' > "$cache_file"
      rm -f "$err_file"
      return 0
    fi
    # Permanent: the number is not a PR. Cache the sentinel so we never retry it.
    if grep -qiE 'could not resolve to a (pullrequest|pull request)' "$err_file"; then
      printf '%s' "$PR_NOTFOUND_SENTINEL" > "$cache_file"
      rm -f "$err_file"
      return 0
    fi
    # Transient (throttling, network): back off and retry, but do NOT cache.
    attempt=$((attempt + 1))
    if [[ $attempt -ge $max_attempts ]]; then
      rm -f "$err_file"
      return 1
    fi
    sleep $((attempt * attempt))   # 1s, 4s, 9s
  done
}

# Bulk-warm the cache before the per-commit loop. Collects every not-yet-cached,
# non-skip PR number in the range and fetches them ~30 at a time via a single
# GraphQL request using field aliases. Batching is what keeps us clear of the
# secondary rate limit. Note: `gh api graphql` exits non-zero whenever the
# response carries an errors[] array (any unresolved PR number triggers that),
# even though the data is present — so we branch on ".data" presence, NOT the
# exit status.
prefetch_pr_cache() {
  command -v gh >/dev/null 2>&1 || return 0

  # Candidate PR numbers: non-skip commits whose PR isn't cached yet.
  local -a prs=()
  local pr
  while IFS= read -r pr; do
    [[ -n $pr ]] && prs+=("$pr")
  done < <(
    git log "$FROM_TAG..$TO_REF" --format="%h|%aN|%aE|%s" \
      | while IFS='|' read -r _h _author _email _subject; do
          [[ -z $_subject ]] && continue

          _pr=$(extract_pr_number "$_subject"); [[ -z $_pr ]] && continue
          _cat=$(extract_category "$_subject")
          _skip=$(classify_skip "$_cat" "$_author" "$_subject"); [[ -n $_skip ]] && continue
          _cache_file="$PR_CACHE_DIR/${PR_REPO//\//_}-${_pr}.json"
          if [[ -s $_cache_file ]] && jq -e \
              '.__notfound__ == true or (has("author_login") and has("author_type"))' \
              "$_cache_file" >/dev/null 2>&1; then
            continue
          fi
          echo "$_pr"
        done | sort -un
  )
  [[ ${#prs[@]} -eq 0 ]] && return 0

  local owner="${PR_REPO%%/*}" name="${PR_REPO##*/}"
  local batch_size=30 i=0 n=${#prs[@]} cf node p j q resp
  local qfile; qfile=$(mktemp)
  while [[ $i -lt $n ]]; do
    q='query($owner:String!,$name:String!){repository(owner:$owner,name:$name){'
    local -a slice=()
    for ((j = 0; j < batch_size && i + j < n; j++)); do
      p=${prs[$((i + j))]}
      slice+=("$p")
      q+="p${p}: pullRequest(number:${p}){number title body author{login __typename} additions deletions changedFiles labels(first:30){nodes{name}} files(first:100){nodes{path}}} "
    done
    q+='}}'
    printf '%s' "$q" > "$qfile"

    resp=$(gh api graphql -F owner="$owner" -F name="$name" -F query=@"$qfile" 2>/dev/null || true)
    if printf '%s' "$resp" | jq -e '.data.repository' >/dev/null 2>&1; then
      for p in "${slice[@]}"; do
        cf="$PR_CACHE_DIR/${PR_REPO//\//_}-${p}.json"
        node=$(printf '%s' "$resp" | jq -c --arg k "p${p}" '.data.repository[$k] // null')
        if [[ $node == "null" || -z $node ]]; then
          printf '%s' "$PR_NOTFOUND_SENTINEL" > "$cf"   # resolved: not a PR
        else
          printf '%s' "$node" | jq -c "$GQL_TO_PRVIEW" > "$cf"
        fi
      done
    fi
    # A whole-batch failure leaves its PRs uncached on purpose: the per-PR
    # fallback in the main loop retries them individually with backoff.
    i=$((i + batch_size))
    sleep 1
  done
  rm -f "$qfile"
}

# Default values
FORMAT="oneline"
GROUP_BY_AUTHOR=false
NEW_AUTHORS_ONLY=false
MERGE_ONLY=false
JSON_OUTPUT=false
CUSTOM_TAG=""
TO_REF="HEAD"
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
    --tag)
      CUSTOM_TAG="$2"
      shift 2
      ;;
    --to)
      TO_REF="$2"
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
  echo "  --tag TAG             Start of range: previous release tag (default: auto-detect latest)"
  echo "  --to REF              End of range: target ref (default: HEAD)"
  echo "  -h, --help            Show this help message"
  echo ""
  echo "Examples:"
  echo "  $(basename "$0")                      # Show commits since last release"
  echo "  $(basename "$0") -f medium            # Show with more detail"
  echo "  $(basename "$0") -a                   # Group by author"
  echo "  $(basename "$0") --new-authors        # Show only new authors since release"
  echo "  $(basename "$0") -m                   # Show only merge commits"
  echo "  $(basename "$0") --tag v1.16.0        # Start from a specific tag"
  echo "  $(basename "$0") --tag v1.16.0 --to v1.17.0  # Full tag-to-tag range"
  exit 0
fi

# Determine the repository root
if ! git rev-parse --git-dir > /dev/null 2>&1; then
  echo -e "${RED}Error: Not in a git repository${NC}"
  exit 1
fi

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT" || exit 1

# Find the latest release tag (start of range)
if [[ -z $CUSTOM_TAG ]]; then
  # Look for release tags matching v<digit>.<digit>.<digit> (excluding -dev, -rc, etc.).
  # `|| true`: under `set -o pipefail` a no-match `grep` fails the pipeline, which
  # would abort the assignment via `set -e` before the fallback below can run.
  FROM_TAG=$(git tag -l 'v[0-9]*' --sort=-version:refname | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | head -1) || true

  if [[ -z $FROM_TAG ]]; then
    # Fallback: get any v-prefixed tag
    FROM_TAG=$(git tag -l 'v[0-9]*' --sort=-version:refname | head -1)
  fi

  if [[ -z $FROM_TAG ]]; then
    echo -e "${RED}Error: No release tags found${NC}"
    exit 1
  fi
else
  FROM_TAG=$CUSTOM_TAG
fi

# Verify both ends of the range exist
if ! git rev-parse "$FROM_TAG" > /dev/null 2>&1; then
  echo -e "${RED}Error: Tag '$FROM_TAG' not found${NC}"
  exit 1
fi

if ! git rev-parse "$TO_REF" > /dev/null 2>&1; then
  echo -e "${RED}Error: Ref '$TO_REF' not found${NC}"
  exit 1
fi

# Get commit count and summary
COMMIT_COUNT=$(git rev-list --count "$FROM_TAG..$TO_REF")

if [[ $JSON_OUTPUT == false ]]; then
  echo -e "${BLUE}Commits since ${GREEN}${FROM_TAG}${BLUE}:${NC} ${GREEN}${COMMIT_COUNT}${NC}"
  echo ""
fi

# Build git log command
MERGE_FLAG=""
if [[ $MERGE_ONLY == true ]]; then
  MERGE_FLAG="--merges"
fi

case $FORMAT in
  oneline)
    LOG_CMD="git log $FROM_TAG..$TO_REF $MERGE_FLAG --oneline"
    ;;
  short)
    LOG_CMD="git log $FROM_TAG..$TO_REF $MERGE_FLAG --format='%h%n%an%n%s%n'"
    ;;
  medium)
    LOG_CMD="git log $FROM_TAG..$TO_REF $MERGE_FLAG --format='%H%n%an <%ae>%naD%n%s%n%b%n'"
    ;;
  full)
    LOG_CMD="git log $FROM_TAG..$TO_REF $MERGE_FLAG"
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
    echo -e "${YELLOW}New authors since ${FROM_TAG}:${NC}"
    # Get all authors before the release tag
    HISTORICAL_AUTHORS=$(git log --all --before="$(git log -1 --format=%aI $FROM_TAG)" --format=format:"%aN" | sort | uniq)
    # Get all authors since the release tag
    NEW_RELEASE_AUTHORS=$(git log "$FROM_TAG..$TO_REF" --format=format:"%aN")
    # Find authors that are new (not in historical authors)
    echo "$NEW_RELEASE_AUTHORS" | sort | uniq | while read author; do
      if ! echo "$HISTORICAL_AUTHORS" | grep -Fxq "$author"; then
        COMMIT_COUNT=$(echo "$NEW_RELEASE_AUTHORS" | grep -c "^$author$")
        echo "  $COMMIT_COUNT commits - $author"
      fi
    done | sort -rn
  else
    echo -e "${YELLOW}Commits by author:${NC}"
    git log "$FROM_TAG..$TO_REF" --format=format:"%aN" | sort | uniq -c | sort -rn | while read count author; do
      echo "  $count commits - $author"
    done
  fi
elif [[ $JSON_OUTPUT == true ]]; then
  # Generate JSON output with categorization

  # Enrichment requires the GitHub CLI
  if ! command -v gh > /dev/null 2>&1; then
    echo -e "${YELLOW}Warning: 'gh' was not found; continuing without PR metadata${NC}" >&2
  fi

  # Per-PR cache so re-runs don't re-hit the GitHub API
  PR_CACHE_DIR="${TMPDIR:-/tmp}/mp-pr-cache"
  mkdir -p "$PR_CACHE_DIR"

  # Self-heal a poisoned cache: older runs wrote a bare "{}" for a FAILED fetch,
  # but "{}" is non-empty so the "[[ ! -s ]]" guard below treats it as a valid
  # hit and never retries. Purge those so transient failures can be retried;
  # real results and the notfound sentinel are kept.
  for _cf in "$PR_CACHE_DIR/${PR_REPO//\//_}"-*.json; do
    [[ -e $_cf ]] || continue
    [[ $(cat "$_cf" 2>/dev/null) == '{}' ]] && rm -f "$_cf"
  done

  # Track enrichment outcomes so silent gh failures (auth, rate limits, wrong
  # PR_REPO) can be surfaced as a warning instead of producing thin records.
  # ENRICH_NOTFOUND is kept separate: a number GitHub says is not a PR (an issue
  # or cross-repo ref) is a resolved result, not a failure, and must not trip
  # the "all failed" abort.
  ENRICH_ATTEMPTED=0
  ENRICH_FAILED=0
  ENRICH_NOTFOUND=0

  # Contributor detection is release-aware and stateless: a contributor is new
  # iff none of their PRs shipped in a published release before the target.
  # SHIPPED_PR holds every PR number already shipped; the per-author arrays feed
  # classify_new_login. No persistent ledger is required.
  declare -A SHIPPED_PR=()
  declare -A LOGIN_STATUS=()
  declare -A AUTHOR_INRANGE_PRS=()
  declare -A AUTHOR_IS_BOT=()
  PRIOR_RELEASE_COUNT=0
  CONTRIBUTOR_UNRESOLVED=0
  compute_prior_shipped_prs

  # Warm the cache in bulk BEFORE the per-commit loop. This is the key fix for
  # mass-enrichment failures: batching ~30 PRs per GraphQL request keeps us well
  # clear of the secondary rate limit that a per-PR fetch loop would trip.
  prefetch_pr_cache

  # One JSON object per line, slurped into an array at the end
  RECORDS_FILE=$(mktemp)
  trap "rm -f $RECORDS_FILE" EXIT

  while IFS=$'\n' read -r line; do
    [[ -z $line ]] && continue

    hash=$(echo "$line" | cut -d'|' -f1)
    author=$(echo "$line" | cut -d'|' -f2)
    author_email=$(echo "$line" | cut -d'|' -f3)
    subject=$(echo "$line" | cut -d'|' -f4-)

    # Apply merge filter
    if [[ $MERGE_ONLY == true ]]; then
      IS_MERGE=$(git log -1 "$hash" --format=%b | grep -c "^Merge" || echo 0)
      if [[ $IS_MERGE -eq 0 ]]; then
        continue
      fi
    fi

    pr_number=$(extract_pr_number "$subject")
    category=$(extract_category "$subject")

    # Early-exit classification: skip release-notes noise before enrichment.
    skip_reason=$(classify_skip "$category" "$author" "$subject")
    skip=false
    [[ -n $skip_reason ]] && skip=true

    # Fetch PR metadata (title, body, labels, diffstat, files). Skipped commits
    # are never enriched: no gh call, no body in context. The bulk prefetch
    # above usually populated the cache already; the per-PR call here is a
    # backoff-guarded fallback for anything a batch missed.
    enrich="{}"
    if [[ -n $pr_number ]]; then
      cache_file="$PR_CACHE_DIR/${PR_REPO//\//_}-${pr_number}.json"
      # Older cache entries predate authoritative PR-author fields. Remove
      # them so the fallback fetch cannot silently use commit attribution.
      if [[ -s $cache_file ]] && ! jq -e \
          '.__notfound__ == true or (has("author_login") and has("author_type"))' \
          "$cache_file" >/dev/null 2>&1; then
        rm -f "$cache_file"
      fi
      # fetch_one_pr never persists "{}" on failure, so a transient error can't
      # poison the cache and block a later retry.
      if [[ ! -s $cache_file ]] && command -v gh >/dev/null 2>&1; then
        fetch_one_pr "$pr_number" "$cache_file" || true
      fi

      if [[ -s $cache_file ]]; then
        enrich=$(cat "$cache_file")
        if printf '%s' "$enrich" | jq -e '.__notfound__ == true' >/dev/null 2>&1; then
          # Resolved: GitHub says this number isn't a PR (e.g. an issue). Treat
          # as "no enrichment" WITHOUT counting it as a failure.
          enrich="{}"
          ENRICH_NOTFOUND=$((ENRICH_NOTFOUND + 1))
        else
          # Count a failure when the fetch produced no usable title.
          ENRICH_ATTEMPTED=$((ENRICH_ATTEMPTED + 1))
          if [[ $(printf '%s' "$enrich" | jq -r '.title // ""' 2>/dev/null) == "" ]]; then
            ENRICH_FAILED=$((ENRICH_FAILED + 1))
          fi
        fi
      else
        # No cache after the fallback: a transient failure we deliberately did
        # not persist. Count it so the summary can flag rate limiting.
        ENRICH_ATTEMPTED=$((ENRICH_ATTEMPTED + 1))
        ENRICH_FAILED=$((ENRICH_FAILED + 1))
      fi
    fi

    # Prefer the authoritative PR author over the Git commit author. The
    # latter is frequently the merger for squash merges and the cherry-picker
    # for maintenance releases. PR-less commits do not create contributor
    # claims because there is no authoritative PR author to credit.
    pr_author_login=""
    pr_author_type=""
    if [[ $enrich != "{}" ]]; then
      pr_author_login=$(echo "$enrich" | jq -r '.author_login // ""' 2>/dev/null || echo "")
      pr_author_type=$(echo "$enrich" | jq -r '.author_type // ""' 2>/dev/null || echo "")
    fi
    # Record the authoritative PR author for stateless, release-aware
    # contributor detection performed after the loop. PR-less commits never
    # create a contributor claim (no authoritative author to credit).
    author_login=""
    if [[ -n $pr_author_login ]]; then
      author_login="$pr_author_login"
      if [[ $pr_author_type == "Bot" || $pr_author_login == *"[bot]" ]]; then
        AUTHOR_IS_BOT["$pr_author_login"]=1
      fi
      [[ -n $pr_number ]] && AUTHOR_INRANGE_PRS["$pr_author_login"]+=" $pr_number"
    fi

    # Classify using the PR body when available, not just the subject line
    body_for_type=""
    if [[ $enrich != "{}" ]]; then
      body_for_type=$(echo "$enrich" | jq -r '.body // ""' 2>/dev/null || echo "")
    fi
    commit_type=$(detect_commit_type "$subject" "$body_for_type")

    # Build the record; merge enrichment fields when present. contributor_status
    # and is_new_author are injected in a post-pass once every author is known.
    jq -nc \
      --arg hash "$hash" \
      --arg author "$author" \
      --arg author_login "$author_login" \
      --arg subject "$subject" \
      --arg category "$category" \
      --arg type "$commit_type" \
      --argjson pr "${pr_number:-null}" \
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
        skip: $skip,
        skip_reason: (if $skip_reason == "" then null else $skip_reason end)
      }
      + (if $author_login != "" then {author_login: $author_login} else {} end)
      + (if ($enrich | type) == "object" and ($enrich | keys | length) > 0 then {
          pr_title: ($enrich.title // null),
          pr_body: (($enrich.body // "") | .[0:4000]),
          pr_author_login: ($enrich.author_login // null),
          pr_author_type: ($enrich.author_type // null),
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
  done < <(git log "$FROM_TAG..$TO_REF" --format="%h|%aN|%aE|%s")

  # Surface enrichment problems on stderr (stdout stays valid JSON). Permanent
  # "not a PR" results are excluded from these counts (see ENRICH_NOTFOUND), so
  # any failure here is transient by construction.
  if [[ $ENRICH_ATTEMPTED -gt 0 && $ENRICH_FAILED -gt 0 ]]; then
    if [[ $ENRICH_FAILED -eq $ENRICH_ATTEMPTED ]]; then
      echo -e "${RED}Error: enrichment failed for all ${ENRICH_ATTEMPTED} PRs.${NC}" >&2
    else
      echo -e "${YELLOW}Warning: enrichment failed for ${ENRICH_FAILED}/${ENRICH_ATTEMPTED} PRs (those records fall back to the subject line).${NC}" >&2
    fi
    # Most likely cause is GitHub's SECONDARY rate limit from bulk PR fetches: it
    # does NOT decrement the `rate_limit` quota, so `used:0` there is not proof
    # of health. The cache is written only on success, so simply re-running
    # retries exactly the misses. If it persists, suspect auth or a wrong repo.
    echo -e "${YELLOW}  Likely secondary rate limiting; re-run to retry the misses via cache. If it persists, verify 'gh auth status' and PR_REPO='${PR_REPO}'.${NC}" >&2
  fi

  # Classify every distinct PR author once, statelessly (see classify_new_login),
  # then inject contributor_status / is_new_author into the records. Bots are
  # never new contributors; an unresolved GitHub history marks the run
  # incomplete rather than guessing "new".
  STATUS_MAP=$(mktemp)
  echo '{}' > "$STATUS_MAP"
  for _login in "${!AUTHOR_INRANGE_PRS[@]}"; do
    if [[ -n ${AUTHOR_IS_BOT[$_login]:-} ]]; then
      _st="bot"
    else
      _st=$(classify_new_login "$_login")
    fi
    [[ $_st == "unresolved" ]] && CONTRIBUTOR_UNRESOLVED=$((CONTRIBUTOR_UNRESOLVED + 1))
    jq -c --arg l "$_login" --arg s "$_st" '.[$l] = $s' "$STATUS_MAP" > "${STATUS_MAP}.tmp" \
      && mv "${STATUS_MAP}.tmp" "$STATUS_MAP"
  done

  jq -c --slurpfile smap "$STATUS_MAP" '
    ($smap[0]) as $status
    | (.pr_author_login // null) as $login
    | .contributor_status = (if $login == null then "not-applicable"
                             else ($status[$login] // "not-applicable") end)
    | .is_new_author = (.contributor_status == "true")
  ' "$RECORDS_FILE" > "${RECORDS_FILE}.classified" \
    && mv "${RECORDS_FILE}.classified" "$RECORDS_FILE"
  rm -f "$STATUS_MAP"

  jq -n \
    --arg tag "$FROM_TAG" \
    --arg to "$TO_REF" \
    --argjson count "$COMMIT_COUNT" \
    --arg gen "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    --argjson prior_releases "$PRIOR_RELEASE_COUNT" \
    --argjson contributor_unresolved "$CONTRIBUTOR_UNRESOLVED" \
    --slurpfile commits "$RECORDS_FILE" \
    '{
      metadata: {
        release_tag: $tag,
        range_end: $to,
        total_commits: $count,
        generated_at: $gen,
        contributor_detection: {
          method: "first-shipped-release",
          prior_releases: $prior_releases,
          unresolved: $contributor_unresolved,
          complete: ($prior_releases > 0 and $contributor_unresolved == 0)
        }
      },
      commits: $commits
    }'

else
  # Execute the log command for human-readable output
  eval "$LOG_CMD"
fi
