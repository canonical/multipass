#!/usr/bin/env python3
"""Assign one eligible Multipass team member to review a pull request.

This automation runs when the GitHub team ``multipass`` is requested for pull
request review. It builds a candidate pool from the team members, removes the PR
author and configured exclusions, and ranks candidates by three per-candidate
signals derived from the GitHub API, each decayed with a continuous per-day
exponential decay on the relevant timestamp:

* review load -- for every Approved / Changes-Requested review the candidate
  submitted, the reviewed PR's total changed lines contribute
  ``count_decay_weight(review_index, lines) ** EXP_PR_SIZE`` under the same
  per-day time decay as the line metrics; the counted line number is discounted
  by how many times the author had already reviewed that PR (re-reviews count
  for less, with larger PRs discounted more slowly);
* assigned-PR lines -- decayed lines of PRs whose review is requested from them; and
* created-PR lines -- decayed lines of ready-to-review PRs they authored.

The external ``review-stats`` tool (which only reports review counts) is kept in
place -- the workflow still clones and builds it -- but is not used for scoring.
A scoring equation then picks the lowest-load reviewer.

The scoring equation intentionally raises ``NotImplementedError`` until the
project fills in its final ranking formula, so the workflow will not assign
anyone before that placeholder is replaced.
"""

from __future__ import annotations

import dataclasses
import hashlib
import json
import math
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from collections.abc import Mapping
from datetime import datetime, timedelta, timezone
from typing import Any

# Scoring-equation coefficient tuned by the project owner.
EXP_PR_SIZE = 1.2  # weight/exponent applied to the PR total-lines term

# Continuous per-day decay applied to every active metric, using each item's own
# timestamp (review submitted_at for review load; PR created_at for line loads).
DECAY_RATE_PER_DAY = 0.1  # base exponential decay rate per day; weight = exp(-rate * age_days)
LOOKBACK_DAYS = 30  # history window for GitHub API searches; older items are dropped
# Review states that count toward the line-weighted review load.
REVIEW_STATES = ("APPROVED", "CHANGES_REQUESTED")
# Rate for the decay applied to the counted line count on repeat reviews of the
# same PR by the same author (see ``count_decay_weight``): effective lines =
# lines * exp(-REVIEW_COUNT_DECAY * review_index / sqrt(lines)), so re-reviews
# count for less, and larger PRs' counts decay more slowly with each re-review.
REVIEW_COUNT_DECAY = 0.5
# Search ``author:`` qualifiers for PRs opened by the Copilot coding agent on a
# human's behalf; such PRs are credited to their human assignee's created load.
COPILOT_AUTHOR_LOGINS = ("app/copilot-swe-agent", "app/copilot")

EXCLUDED_PEOPLE = frozenset()  # logins never assigned (fill later)
# Reserved for future per-file line exclusions; the PR total is currently used as-is.
LOCKFILE_EXCLUDE_PATTERNS = (
    "Cargo.lock",
    "pubspec.lock",
)

GITHUB_API = "https://api.github.com"
TEAM_NAME = "multipass"


@dataclasses.dataclass(frozen=True)
class EventContext:
    """Details extracted from the pull_request review_requested event."""

    owner: str
    repo: str
    pr_number: int
    pr_author: str
    requested_team_slug: str


@dataclasses.dataclass(frozen=True)
class CandidateInputs:
    """All inputs supplied to the scoring equation for one candidate."""

    login: str
    decayed_review_load: float
    # Line-count signals not provided by review-stats; computed from the GitHub
    # API and decayed with the same weekly buckets (older PRs weigh less).
    assigned_lines_decayed: float
    created_lines_decayed: float


class GitHubClient:
    """Small urllib-based GitHub REST client with bearer auth and pagination."""

    def __init__(self, token: str) -> None:
        self.token = token

    def request(self, method: str, path_or_url: str, body: Mapping[str, Any] | None = None) -> Any:
        """Send one GitHub REST request and return decoded JSON, if any."""

        url = path_or_url if path_or_url.startswith("https://") else f"{GITHUB_API}{path_or_url}"
        data = None if body is None else json.dumps(body).encode("utf-8")
        headers = {
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {self.token}",
            "X-GitHub-Api-Version": "2022-11-28",
        }
        if data is not None:
            headers["Content-Type"] = "application/json"

        request = urllib.request.Request(url, data=data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                payload = response.read()
        except urllib.error.HTTPError as err:
            detail = err.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"GitHub REST {method} {url} failed: HTTP {err.code}: {detail}") from err
        except urllib.error.URLError as err:
            raise RuntimeError(f"GitHub REST {method} {url} failed: {err.reason}") from err

        if not payload:
            return None
        return json.loads(payload.decode("utf-8"))

    def search_issues(self, query: str) -> list[dict[str, Any]]:
        """Return all issue/PR items matching a GitHub search query.

        Handles the search response envelope (``{"total_count", "items"}``) and
        follows ``Link`` pagination.
        """

        items: list[dict[str, Any]] = []
        url = f"{GITHUB_API}/search/issues?q={urllib.parse.quote(query)}&per_page=100"
        while url:
            request = urllib.request.Request(
                url,
                headers={
                    "Accept": "application/vnd.github+json",
                    "Authorization": f"Bearer {self.token}",
                    "X-GitHub-Api-Version": "2022-11-28",
                },
                method="GET",
            )
            try:
                with urllib.request.urlopen(request, timeout=60) as response:
                    payload = response.read()
                    link = response.headers.get("Link", "")
            except urllib.error.HTTPError as err:
                detail = err.read().decode("utf-8", errors="replace")
                raise RuntimeError(f"GitHub search {url} failed: HTTP {err.code}: {detail}") from err
            except urllib.error.URLError as err:
                raise RuntimeError(f"GitHub search {url} failed: {err.reason}") from err

            data = json.loads(payload.decode("utf-8")) if payload else {}
            items.extend(data.get("items", []) or [])
            url = next_link(link)
        return items

    def paginate(self, path: str) -> list[Any]:
        """Fetch all pages for a GitHub REST list endpoint using Link headers."""

        items: list[Any] = []
        url = f"{GITHUB_API}{path}"
        while url:
            request = urllib.request.Request(
                url,
                headers={
                    "Accept": "application/vnd.github+json",
                    "Authorization": f"Bearer {self.token}",
                    "X-GitHub-Api-Version": "2022-11-28",
                },
                method="GET",
            )
            try:
                with urllib.request.urlopen(request, timeout=60) as response:
                    payload = response.read()
                    link = response.headers.get("Link", "")
            except urllib.error.HTTPError as err:
                detail = err.read().decode("utf-8", errors="replace")
                raise RuntimeError(f"GitHub REST GET {url} failed: HTTP {err.code}: {detail}") from err
            except urllib.error.URLError as err:
                raise RuntimeError(f"GitHub REST GET {url} failed: {err.reason}") from err

            page = json.loads(payload.decode("utf-8")) if payload else []
            if not isinstance(page, list):
                raise RuntimeError(f"GitHub REST GET {url} returned a non-list page")
            items.extend(page)
            url = next_link(link)
        return items


def next_link(link_header: str) -> str | None:
    """Return the URL for rel=next from a GitHub Link header."""

    for part in link_header.split(","):
        match = re.match(r'\s*<([^>]+)>;\s*rel="([^"]+)"', part)
        if match and match.group(2) == "next":
            return match.group(1)
    return None


def load_event_context() -> EventContext | None:
    """Load the GitHub event and extract repository, PR, author, and team data."""

    event_path = os.environ["GITHUB_EVENT_PATH"]
    with open(event_path, encoding="utf-8") as event_file:
        event = json.load(event_file)

    requested_team = event.get("requested_team") or {}
    if requested_team.get("name") != TEAM_NAME:
        print(f"Notice: requested team is not {TEAM_NAME}; nothing to do.")
        return None

    repository = os.environ["GITHUB_REPOSITORY"]
    owner, repo = repository.split("/", 1)
    pull_request = event["pull_request"]
    return EventContext(
        owner=owner,
        repo=repo,
        pr_number=int(pull_request["number"]),
        pr_author=pull_request["user"]["login"],
        requested_team_slug=requested_team.get("slug") or TEAM_NAME,
    )


def enumerate_candidates(client: GitHubClient, context: EventContext) -> list[str]:
    """Return eligible team-member logins, excluding configured people and the PR author."""

    path = f"/orgs/{context.owner}/teams/{context.requested_team_slug}/members?per_page=100"
    members = client.paginate(path)
    excluded = {login.casefold() for login in EXCLUDED_PEOPLE}
    excluded.add(context.pr_author.casefold())
    candidates = sorted(
        member["login"]
        for member in members
        if member.get("login") and member["login"].casefold() not in excluded
    )
    return candidates

def time_decay_weight(age_days: float, rate: float = DECAY_RATE_PER_DAY) -> float:
    """Continuous per-day exponential decay: ``exp(-rate * age_days)``.

    ``age_days`` is clamped at 0 so future-dated timestamps do not amplify weight.
    """

    return math.exp(-rate * max(age_days, 0.0))

def count_decay_weight(review_count: int, lines: int) -> float:
    """Effective reviewed line count after decaying for repeat reviews.

    Returns ``lines * exp(-REVIEW_COUNT_DECAY * review_count / sqrt(lines))``:
    the PR's line count discounted by ``review_count`` -- the number of the same
    author's prior qualifying reviews on that PR (0-based, so the first review is
    undiscounted) -- so re-reviews count for less. Dividing the rate by
    ``sqrt(lines)`` makes larger PRs' counts decay more slowly with each
    re-review than smaller ones.
    """

    return lines * math.exp(-REVIEW_COUNT_DECAY * review_count / math.sqrt(lines))


def _parse_iso8601(value: str) -> datetime:
    """Parse a GitHub ISO-8601 timestamp (``...Z``) into an aware datetime."""

    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def compute_decayed_pr_line_loads(
    client: GitHubClient, context: EventContext, candidates: list[str]
) -> tuple[dict[str, float], dict[str, float]]:
    """Compute per-candidate decayed line counts for assigned and created PRs.

    ``review-stats`` does not expose line counts, so we derive them here. For each
    candidate we search the repository for PRs created within ``LOOKBACK_DAYS`` in
    two roles:

    * created -- ready-to-review (non-draft) PRs the candidate authored
      (``author:`` with ``draft:false``), plus non-draft PRs opened by the Copilot
      coding agent on the candidate's behalf (``author:<copilot> assignee:<login>``);
      and
    * assigned -- PRs where the candidate's review is requested
      (``review-requested:`` qualifier).

    Each matching PR contributes ``(additions + deletions) ** EXP_PR_SIZE`` scaled
    by a continuous per-day time decay. The created role searches and decays by
    ``created_at``; the assigned role searches and decays by ``updated_at`` so PRs
    created long ago but with a recent review request still count. Returns
    ``(assigned, created)`` dictionaries keyed by login.
    """

    now = datetime.now(timezone.utc)
    cutoff = (now - timedelta(days=LOOKBACK_DAYS)).date().isoformat()

    line_cache: dict[int, int] = {}

    def total_lines(number: int) -> int:
        if number not in line_cache:
            pull = client.request("GET", f"/repos/{context.owner}/{context.repo}/pulls/{number}")
            line_cache[number] = int(pull.get("additions", 0)) + int(pull.get("deletions", 0))
        return line_cache[number]

    assigned = {login: 0.0 for login in candidates}
    created = {login: 0.0 for login in candidates}

    for login in candidates:
        # (target, search fragment, date qualifier, timestamp field for decay age).
        # created keys off creation; assigned keys off last update so PRs created
        # long ago but with a recent review request are not missed. The created
        # role also credits Copilot-authored PRs opened on this candidate's behalf.
        role_queries: list[tuple[dict[str, float], str, str, str]] = [
            (created, f"author:{login} draft:false", "created", "created_at"),
            (assigned, f"review-requested:{login}", "updated", "updated_at"),
        ]
        role_queries.extend(
            (created, f"author:{copilot} assignee:{login} draft:false", "created", "created_at")
            for copilot in COPILOT_AUTHOR_LOGINS
        )
        seen: dict[int, set[int]] = {id(created): set(), id(assigned): set()}
        for target, fragment, date_qualifier, timestamp_field in role_queries:
            query = f"repo:{context.owner}/{context.repo} type:pr {fragment} {date_qualifier}:>={cutoff}"
            for item in client.search_issues(query):
                timestamp = item.get(timestamp_field)
                number = item.get("number")
                if not timestamp or number is None:
                    continue
                if int(number) in seen[id(target)]:
                    continue  # avoid double-counting a PR matched by multiple queries in the same role
                seen[id(target)].add(int(number))
                age_days = (now - _parse_iso8601(timestamp)).total_seconds() / 86400.0
                lines = total_lines(int(number))
                if lines <= 0:
                    continue
                target[login] += lines ** EXP_PR_SIZE * time_decay_weight(age_days)

    return assigned, created


def compute_decayed_review_lines(
    client: GitHubClient, context: EventContext, candidates: list[str]
) -> dict[str, float]:
    """Line-weighted review load for each candidate, from the GitHub API.

    For every review the candidate submitted with state in ``REVIEW_STATES``
    (Approved / Changes Requested) within ``LOOKBACK_DAYS``, the reviewed PR's
    total changed lines (``additions + deletions``) contribute
    ``count_decay_weight(review_index, lines) ** EXP_PR_SIZE`` scaled by the same
    per-day time decay used for the other line metrics (``time_decay_weight`` at
    ``DECAY_RATE_PER_DAY``, by the review's ``submitted_at``). Reviews on PRs the
    candidate authored are ignored. ``count_decay_weight`` discounts the PR line
    count by ``review_index`` -- the number of the author's prior qualifying
    reviews on that PR (0-based, ordered by submission time) -- as
    ``lines * exp(-REVIEW_COUNT_DECAY * review_index / sqrt(lines))``, so a first
    review counts full lines, re-reviews count for less, and larger PRs' counts
    decay more slowly per re-review. Each qualifying review event still
    contributes separately.
    """

    now = datetime.now(timezone.utc)
    cutoff = (now - timedelta(days=LOOKBACK_DAYS)).date().isoformat()

    line_cache: dict[int, int] = {}

    def total_lines(number: int) -> int:
        if number not in line_cache:
            pull = client.request("GET", f"/repos/{context.owner}/{context.repo}/pulls/{number}")
            line_cache[number] = int(pull.get("additions", 0)) + int(pull.get("deletions", 0))
        return line_cache[number]

    load = {login: 0.0 for login in candidates}

    for login in candidates:
        folded = login.casefold()
        query = f"repo:{context.owner}/{context.repo} type:pr reviewed-by:{login} updated:>={cutoff}"
        for item in client.search_issues(query):
            number = item.get("number")
            if number is None:
                continue
            if (item.get("user") or {}).get("login", "").casefold() == folded:
                continue  # don't credit reviewing one's own PR
            lines = total_lines(int(number))
            if lines <= 0:
                continue
            reviews = client.paginate(
                f"/repos/{context.owner}/{context.repo}/pulls/{int(number)}/reviews?per_page=100"
            )
            # This candidate's qualifying reviews on the PR, oldest first, so the
            # review-count decay indexes from the earliest review.
            submissions = sorted(
                review["submitted_at"]
                for review in reviews
                if (review.get("user") or {}).get("login", "").casefold() == folded
                and review.get("state") in REVIEW_STATES
                and review.get("submitted_at")
            )
            for index, submitted_at in enumerate(submissions):
                age_days = (now - _parse_iso8601(submitted_at)).total_seconds() / 86400.0
                if age_days > LOOKBACK_DAYS:
                    continue
                load[login] += count_decay_weight(index,lines) ** EXP_PR_SIZE * time_decay_weight(age_days)

    return load


def requested_candidate_logins(pull: Mapping[str, Any], candidates: list[str]) -> list[str]:
    """Return eligible candidates already individually requested on the PR."""

    requested = {
        reviewer.get("login", "").casefold()
        for reviewer in pull.get("requested_reviewers", []) or []
        if reviewer.get("login")
    }
    return sorted(login for login in candidates if login.casefold() in requested)


def build_candidate_inputs(
    client: GitHubClient, context: EventContext, candidates: list[str],
) -> list[CandidateInputs]:
    """Build scoring inputs for each eligible candidate.

    Combines the line-weighted review load (derived from the candidate's
    Approved/Changes-Requested reviews) with the decayed assigned- and created-PR
    line counts. ``review-stats`` is intentionally not consulted here.
    """

    loads = compute_decayed_review_lines(client, context, candidates)
    assigned_lines, created_lines = compute_decayed_pr_line_loads(client, context, candidates)
    inputs = []
    for login in candidates:
        inputs.append(
            CandidateInputs(
                login=login,
                decayed_review_load=loads[login],
                assigned_lines_decayed=assigned_lines[login],
                created_lines_decayed=created_lines[login],
            )
        )
    return inputs


def score_candidate(inputs: CandidateInputs) -> float:
    """Return the candidate's assignment score; lower score is assigned.

    Available inputs:
    * ``inputs.login``: GitHub login for the candidate.
    * ``inputs.decayed_review_load``: line-weighted review load -- the decayed
      sum over the candidate's Approved/Changes-Requested reviews of
      ``count_decay_weight(review_index, lines) ** EXP_PR_SIZE``, where the
      counted line number is discounted by how many times the candidate had
      already reviewed that PR (``lines * exp(-REVIEW_COUNT_DECAY *
      review_index / sqrt(lines))``), all under the standard per-day time decay.
    * ``inputs.assigned_lines_decayed``: decayed total changed lines of PRs whose
      review is currently requested from this candidate.
    * ``inputs.created_lines_decayed``: decayed total changed lines of
      ready-to-review (non-draft) PRs this candidate authored -- or that Copilot
      opened on their behalf -- within the window.
    The tunable coefficient is:
    * ``EXP_PR_SIZE`` for the PR total-lines term.
    """

    return inputs.created_lines_decayed - inputs.assigned_lines_decayed - inputs.decayed_review_load


def tie_break_key(context: EventContext, candidate: CandidateInputs) -> tuple[float, float, str]:
    """Return deterministic ordering key for scored candidates."""

    score = score_candidate(candidate)
    digest = hashlib.sha256(
        f"multipass-review-assignment-v1:{context.pr_number}:{candidate.login}".encode("utf-8")
    ).hexdigest()
    return (score, candidate.decayed_review_load, digest)


def assign_reviewer(client: GitHubClient, context: EventContext, candidate: str) -> None:
    """Request review from the selected individual and drop the team request."""

    path = f"/repos/{context.owner}/{context.repo}/pulls/{context.pr_number}/requested_reviewers"
    client.request("POST", path, {"reviewers": [candidate]})
    print(f"Requested PR #{context.pr_number} review from {candidate}.")

    client.request("DELETE", path, {"team_reviewers": [context.requested_team_slug]})
    print(f"Removed team '{context.requested_team_slug}' review request from PR #{context.pr_number}.")


def main() -> int:
    """Run the reviewer-assignment orchestration."""

    context = load_event_context()
    if context is None:
        return 0

    token = os.environ["GITHUB_TOKEN"]
    client = GitHubClient(token)
    candidates = enumerate_candidates(client, context)
    if not candidates:
        print("Notice: no eligible Multipass team members remain after exclusions and author removal.")
        return 0

    # One fetch of the triggering PR gives both idempotency info and its size.
    pull = client.request("GET", f"/repos/{context.owner}/{context.repo}/pulls/{context.pr_number}")
    eligible_requested = requested_candidate_logins(pull, candidates)
    if eligible_requested:
        print(
            "Notice: an eligible Multipass reviewer is already individually requested "
            f"({eligible_requested[0]}); not adding another."
        )
        return 0

    candidate_inputs = build_candidate_inputs(client, context, candidates)
    chosen = min(candidate_inputs, key=lambda candidate: tie_break_key(context, candidate))
    assign_reviewer(client, context, chosen.login)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except NotImplementedError as err:
        print(f"Error: {err}", file=sys.stderr)
        raise
    except Exception as err:  # noqa: BLE001 - keep workflow failures clear without third-party logging.
        print(f"Error: {err}", file=sys.stderr)
        raise SystemExit(1) from err
