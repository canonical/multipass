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
  # Extract PR number from squash-merge format like "(#5078)", or from
  # merge-commit format like "Merge pull request #5078 from ..." (used heavily
  # before squash merging became the norm, e.g. the v1.15..v1.16 era).
  if [[ $subject =~ \(#([0-9]+)\) ]]; then
    echo "${BASH_REMATCH[1]}"
  elif [[ $subject =~ ^Merge\ pull\ request\ \#([0-9]+) ]]; then
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

  # Nothing matched: keep it (deps/cmake/build/docs are evaluated downstream,
  # not skipped, because human-authored ones can be user-facing, e.g. runtime
  # upgrades or installability fixes).
  echo ""
}

# Resolve a git author identity (name + email) to a GitHub login, using the
# commit-search API. The local git author name alone is unreliable for
# crediting: squash-merges and cherry-picks routinely record a different
# author than the person who wrote the change. We search GitHub for commits
# by this author and take the login GitHub reports. Echoes the login, or
# nothing when unresolvable.
resolve_github_login() {
  local name="$1"
  local email="$2"
  local cache_key
  if command -v shasum >/dev/null 2>&1; then
    cache_key=$(printf '%s' "${name}<${email}>" | shasum | awk '{print $1}')
  else
    cache_key=$(printf '%s' "${name}<${email}>" | sha1sum | awk '{print $1}')
  fi
  local cache_file="$PR_CACHE_DIR/${PR_REPO//\//_}-author-${cache_key}.txt"

  if [[ -s $cache_file ]]; then
    cat "$cache_file"
    return
  fi

  local login=""
  # Name search is the primary path: it works for noreply emails and for
  # people whose canonical email isn't linked to their GitHub account. Quote
  # the name so multi-word names aren't split into free-text terms.
  local name_q
  name_q=$(jq -rn --arg n "repo:${PR_REPO} author-name:\"$name\"" '$n|@uri')
  login=$(gh api "search/commits?q=${name_q}" \
    --jq '.items[0].author.login // ""' 2>/dev/null || echo "")

  # Fall back to email search when the name didn't resolve (e.g. the author
  # has since changed their display name).
  if [[ -z $login ]]; then
    local email_q
    email_q=$(jq -rn --arg e "repo:${PR_REPO} author-email:$email" '$e|@uri')
    login=$(gh api "search/commits?q=${email_q}" \
      --jq '.items[0].author.login // ""' 2>/dev/null || echo "")
  fi

  # Only cache a successful resolution; an empty result is transient (offline,
  # rate limit, auth) and must not poison future runs.
  if [[ -n $login ]]; then
    echo "$login" > "$cache_file"
  else
    echo "Warning: could not resolve a GitHub login for '$name' <$email>" >&2
  fi
  echo "$login"
}

# Decide whether an author is a genuinely NEW contributor, defined as "first
# shipped change": the person has no authored commit on any branch dated
# before the release tag, and no merged PR dated before the tag. Local
# commit-author-name matching alone produces both false positives
# (squash-merge/cherry-pick artifacts) and false negatives (committer vs
# author mismatches), so when a local name looks new we verify against GitHub
# history via the author's login.
#
# Args: name, email, tag_date (ISO-8601).
# Echoes "true" or "false".
is_genuinely_new_author() {
  local name="$1"
  local email="$2"
  local tag_date="$3"

  # Cheap local check first: any authored commit before the tag on ANY branch
  # means not new. Match on the full %aN name, exactly as the caller's
  # HISTORICAL_AUTHORS check does, so the two agree.
  if git log --all --before="$tag_date" --format='%aN' 2>/dev/null | grep -Fxq "$name"; then
    echo "false"
    return
  fi

  # Name looked new locally; verify against GitHub before believing it.
  local login
  login=$(resolve_github_login "$name" "$email")
  if [[ -z $login ]]; then
    # Can't resolve a login: fall back to the local verdict.
    echo "true"
    return
  fi

  # Any merged PR by this login before the tag date means not new. GitHub's
  # issue-search sorting does not support "closed", so filter by closed date
  # and select the earliest closedAt locally.
  local earlier search_json
  if ! search_json=$(gh search prs --repo "$PR_REPO" --author "$login" --merged \
      --closed "<$tag_date" \
      --sort created --order asc --json number,closedAt --limit 100 2>/dev/null); then
    echo "Warning: could not search merged PR history for GitHub login '$login'; treating '$name' as locally new" >&2
    echo "true"
    return
  fi

  if ! earlier=$(jq -r --arg d "$tag_date" \
      '[.[] | select(.closedAt != null and .closedAt < $d)] | sort_by(.closedAt) | .[0].number // ""' \
      <<<"$search_json" 2>/dev/null); then
    echo "Warning: could not parse merged PR history for GitHub login '$login'; treating '$name' as locally new" >&2
    echo "true"
    return
  fi
  if [[ -n $earlier ]]; then
    echo "false"
    return
  fi

  echo "true"
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
GQL_TO_PRVIEW='{title, body, labels: [ .labels.nodes[]? | {name} ], additions, deletions, changedFiles, files: [ .files.nodes[]? | {path} ]}'

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
        --json title,body,labels,additions,deletions,changedFiles,files 2>"$err_file"); then
      printf '%s' "$out" > "$cache_file"
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
          [[ -s "$PR_CACHE_DIR/${PR_REPO//\//_}-${_pr}.json" ]] && continue
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
      q+="p${p}: pullRequest(number:${p}){number title body additions deletions changedFiles labels(first:30){nodes{name}} files(first:100){nodes{path}}} "
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

  # New-author cutoff: authors with any commit before this date are "historical".
  # The previous release tag may be a PATCH cut from a release branch that is
  # NOT an ancestor of the range end, in which case the tag's own date sits
  # *after* most of this release's development and would sweep every
  # contributor into "historical" (yielding zero new authors). Use the branch
  # point instead -- the merge-base of the tag and the range end -- so
  # "historical" means "authored before this release line diverged". For a
  # normal ancestor tag the merge-base IS the tagged commit, so this is
  # equivalent to the tag date in the common case.
  CUTOFF_REF=$(git merge-base "$FROM_TAG" "$TO_REF" 2>/dev/null || echo "$FROM_TAG")
  TAG_DATE=$(git log -1 --format=%cI "$CUTOFF_REF")
  HISTORICAL_AUTHORS=$(git log --all --before="$TAG_DATE" --format=format:"%aN" 2>/dev/null | sort | uniq)

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

    is_new_author=false
    author_login=""
    if ! echo "$HISTORICAL_AUTHORS" | grep -Fxq "$author"; then
      # Cheap local check says "new"; verify against GitHub history before
      # believing it (squash-merge/cherry-pick artifacts make local author
      # names unreliable). Only hits the network for candidate-new authors.
      if command -v gh > /dev/null 2>&1; then
        author_login=$(resolve_github_login "$author" "$author_email")
        is_new_author=$(is_genuinely_new_author "$author" "$author_email" "$TAG_DATE")
      else
        is_new_author=true
      fi
    fi

    # Early-exit classification: skip release-notes noise before enrichment.
    skip_reason=$(classify_skip "$category" "$author" "$subject")
    skip=false
    [[ -n $skip_reason ]] && skip=true

    # Fetch PR metadata (title, body, labels, diffstat, files). Skipped commits
    # are never enriched: no gh call, no body in context. The bulk prefetch
    # above usually populated the cache already; the per-PR call here is a
    # backoff-guarded fallback for anything a batch missed.
    enrich="{}"
    if [[ $skip == false && -n $pr_number ]]; then
      cache_file="$PR_CACHE_DIR/${PR_REPO//\//_}-${pr_number}.json"
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
      --arg author_login "$author_login" \
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
      + (if $author_login != "" then {author_login: $author_login} else {} end)
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

  jq -n \
    --arg tag "$FROM_TAG" \
    --arg to "$TO_REF" \
    --argjson count "$COMMIT_COUNT" \
    --arg gen "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    --slurpfile commits "$RECORDS_FILE" \
    '{
      metadata: {
        release_tag: $tag,
        range_end: $to,
        total_commits: $count,
        generated_at: $gen
      },
      commits: $commits
    }'

else
  # Execute the log command for human-readable output
  eval "$LOG_CMD"
fi
