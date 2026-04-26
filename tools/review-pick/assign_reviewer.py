"""
assign_reviewer.py
------------------
Load-balanced PR reviewer assignment using exponential-decay LOC scoring.

Score per person:
    k_p(T) = Σ_submitted  LOC(pr)^α · e^{-λ(T-t)}   [+, you caused work]
           - Σ_assigned   LOC(pr)^α · e^{-λ(T-t)}   [-, you absorbed work]
           - Σ_reviewed   LOC(pr)^α · e^{-λ(T-t)}   [-, you absorbed work]

Assign randomly on a weighted k_p-based distribution
"""

import math
import os
import random
import re
import sys
from collections import defaultdict
from datetime import datetime, timezone

import requests
from dateutil import parser as dateparser

# ---------------------------------------------------------------------------
# Configuration from environment
# ---------------------------------------------------------------------------

TOKEN       = os.environ["GITHUB_TOKEN"]
REPO        = os.environ["REPO"]                        # "owner/repo"
PR_NUMBER   = int(os.environ["PR_NUMBER"])
PR_AUTHOR   = os.environ["PR_AUTHOR"].lower()

HALF_LIFE   = float(os.environ.get("HALF_LIFE_DAYS", "14"))
ALPHA       = float(os.environ.get("LOC_ALPHA", "1.2"))
RESIDUAL    = float(os.environ.get("RESIDUAL_CUTOFF", "0.01"))   # e^{-λΔt} < this → skip
ASSIGN_W    = float(os.environ.get("ASSIGN_WEIGHT", "0.8"))
TEAM_RAW    = os.environ.get("TEAM_MEMBERS", "")
TEAM        = {m.strip().lower() for m in TEAM_RAW.split(",") if m.strip()}

LAMBDA      = math.log(2) / HALF_LIFE    # decay constant

# Residual cutoff: ignore events older than t_cutoff
# e^{-λ·Δt} = RESIDUAL  →  Δt = -ln(RESIDUAL) / λ
CUTOFF_DAYS = -math.log(RESIDUAL) / LAMBDA
print(f"[config] λ={LAMBDA:.4f}/day, half-life={HALF_LIFE}d, "
      f"cutoff={CUTOFF_DAYS:.1f}d, α={ALPHA}, "
      f"assign_weight={ASSIGN_W}")

# ---------------------------------------------------------------------------
# Files to exclude from LOC counting (glob-style patterns → regex)
# ---------------------------------------------------------------------------

EXCLUDE_PATTERNS = [
    r".*\.lock$",
    r"**\pubspec.lock",
    r"package-lock\.json$",
    r"yarn\.lock$",
    r"pnpm-lock\.yaml$",
    r"go\.sum$",
    r"go\.mod$",
    r"Cargo\.lock$",
    r"composer\.lock$",
    r"Pipfile\.lock$",
    r"poetry\.lock$",
    r".*\.min\.js$",
    r".*\.min\.css$",
    r".*_pb2?\.py$",           # generated protobuf
    r".*\.pb\.go$",
    r".*\.generated\.",
    r"vendor/.*",
    r"dist/.*",
    r"build/.*",
    r".*/__snapshots__/.*",
]
EXCLUDE_RE = re.compile("|".join(EXCLUDE_PATTERNS))


def is_excluded(filename: str) -> bool:
    return bool(EXCLUDE_RE.search(filename))


# ---------------------------------------------------------------------------
# GitHub API helpers
# ---------------------------------------------------------------------------

SESSION = requests.Session()
SESSION.headers.update({
    "Authorization": f"Bearer {TOKEN}",
    "Accept": "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
})
BASE = "https://api.github.com"


def gh_get(path: str, params: dict = None) -> list | dict:
    """GET with automatic pagination."""
    url = f"{BASE}{path}"
    results = []
    while url:
        r = SESSION.get(url, params=params)
        if r.status_code == 404:
            return []
        r.raise_for_status()
        data = r.json()
        if isinstance(data, list):
            results.extend(data)
            url = r.links.get("next", {}).get("url")
            params = None   # pagination URL already contains params
        else:
            return data     # single object response
    return results


def gh_post(path: str, body: dict) -> dict:
    r = SESSION.post(f"{BASE}{path}", json=body)
    r.raise_for_status()
    return r.json()


def to_days(iso_str: str) -> float:
    """Convert ISO-8601 timestamp to days since Unix epoch."""
    dt = dateparser.parse(iso_str).astimezone(timezone.utc)
    return dt.timestamp() / 86400.0


def now_days() -> float:
    return datetime.now(timezone.utc).timestamp() / 86400.0

def gh_delete(path: str, body: dict) -> None:
    r = SESSION.delete(f"{BASE}{path}", json=body)
    r.raise_for_status()

# ---------------------------------------------------------------------------
# LOC calculation with exclusions and power-law scaling
# ---------------------------------------------------------------------------

_loc_cache: dict[int, float] = {}


def pr_weighted_loc(pr_number: int) -> float:
    """
    Returns Σ (additions+deletions) for non-excluded files, raised to power α.
    Cached per PR number.
    """
    if pr_number in _loc_cache:
        return _loc_cache[pr_number]

    files = gh_get(f"/repos/{REPO}/pulls/{pr_number}/files")
    raw_loc = sum(
        f.get("additions", 0) + f.get("deletions", 0)
        for f in files
        if not is_excluded(f["filename"])
    )
    # Power-law: small PRs stay small, large PRs grow super-linearly
    weighted = max(raw_loc, 1) ** ALPHA   # floor at 1 to avoid 0^α = 0 edge case
    _loc_cache[pr_number] = weighted
    print(f"  [loc] PR #{pr_number}: raw={raw_loc} LOC → weighted={weighted:.1f}")
    return weighted


# ---------------------------------------------------------------------------
# Event collection
# ---------------------------------------------------------------------------

Event = tuple[float, float, int]   # (timestamp_days, signed_weight, pr_number)


def collect_events(cutoff_days: float) -> dict[str, list[Event]]:
    """
    Returns {login: [(t, signed_w, pr_number), ...]} for all team events
    within the cutoff window.

    signed_w > 0 → submitted (caused work)
    signed_w < 0 → assigned or reviewed (absorbed work)
    """
    events: dict[str, list[Event]] = defaultdict(list)

    # Fetch all closed+open PRs since cutoff
    since_iso = datetime.fromtimestamp(
        (now_days() - cutoff_days) * 86400.0, tz=timezone.utc
    ).isoformat()

    print(f"\n[fetch] Fetching PRs since {since_iso} ...")
    all_prs = gh_get(
        f"/repos/{REPO}/pulls",
        params={"state": "all", "per_page": 100, "sort": "updated", "direction": "desc"},
    )

    processed = 0
    for pr in all_prs:
        t_created = to_days(pr["created_at"])
        if now_days() - t_created > cutoff_days:
            # PRs are sorted by updated desc; once created_at is too old, break
            # (not perfect due to sort=updated, but conservative)
            break

        num       = pr["number"]
        author    = pr["user"]["login"].lower()
        loc       = pr_weighted_loc(num)

        # ── submitted ──────────────────────────────────────────────────────
        events[author].append((t_created, +loc, num))

        # ── review_requested (assignment) ──────────────────────────────────
        timeline = gh_get(
            f"/repos/{REPO}/issues/{num}/timeline",
            params={"per_page": 100},
        )
        for ev in timeline:
            if ev.get("event") != "review_requested":
                continue
            reviewer = ev.get("requested_reviewer")
            if not reviewer:
                continue
            login = reviewer["login"].lower()
            t_assign = to_days(ev["created_at"])
            events[login].append((t_assign, -loc * ASSIGN_W, num))

        # ── completed reviews ──────────────────────────────────────────────
        reviews = gh_get(f"/repos/{REPO}/pulls/{num}/reviews")
        for rev in reviews:
            if rev["state"] in ("APPROVED", "CHANGES_REQUESTED", "COMMENTED"):
                login    = rev["user"]["login"].lower()
                t_review = to_days(rev["submitted_at"])
                events[login].append((t_review, -loc , num))

        processed += 1

    print(f"[fetch] Processed {processed} PRs.")
    return events


# ---------------------------------------------------------------------------
# Score computation via chronological recurrence
# ---------------------------------------------------------------------------

def compute_scores(events: dict[str, list[Event]], T: float) -> dict[str, float]:
    """
    For each person, compute k_p(T) using the O(n) recurrence:

        S₀ = 0
        Sᵢ = S_{i-1} · e^{-λ(tᵢ - t_{i-1})}  +  wᵢ     (chronological order)
        k_p(T) = S_last · e^{-λ(T - t_last)}

    This is mathematically identical to Σ wᵢ · e^{-λ(T-tᵢ)}.
    """
    scores: dict[str, float] = {}
    for person, evs in events.items():
        evs_sorted = sorted(evs, key=lambda e: e[0])   # sort by timestamp

        S      = 0.0
        t_prev = evs_sorted[0][0]

        for (t, w, _pr) in evs_sorted:
            dt = t - t_prev
            S  = S * math.exp(-LAMBDA * dt) + w
            t_prev = t

        # Final decay from last event to now
        scores[person] = S * math.exp(-LAMBDA * (T - t_prev))

    return scores


# ---------------------------------------------------------------------------
# Normalization and weighted selection
# ---------------------------------------------------------------------------

def normalize(scores: dict[str, float]) -> dict[str, float]:
    """Shift so minimum = 0. Higher k' = higher current load."""
    k_min = min(scores.values())
    return {p: s - k_min for p, s in scores.items()}


def weighted_sample(normalized: dict[str, float], exclude: str) -> str:
    """
    Sample proportional to 1/(k' + ε).
    Low load → high weight → more likely to be assigned.
    """
    epsilon = 1e-6
    candidates = {p: s for p, s in normalized.items() if p != exclude}
    if not candidates:
        raise RuntimeError("No eligible reviewers after excluding PR author.")

    weights = {p: 1.0 / (s + epsilon) for p, s in candidates.items()}
    total   = sum(weights.values())
    r       = random.uniform(0, total)
    cumsum  = 0.0
    for p, w in weights.items():
        cumsum += w
        if cumsum >= r:
            return p
    return max(weights, key=weights.get)   # fallback (floating point edge)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    T = now_days()

    # Determine practical cutoff (residual < RESIDUAL at boundary)
    cutoff = CUTOFF_DAYS
    print(f"[main] Cutoff window: {cutoff:.1f} days "
          f"(residual={RESIDUAL*100:.2f}% at boundary)")

    # Collect events
    events = collect_events(cutoff_days=cutoff)

    # If TEAM_MEMBERS is set, restrict to those logins;
    # otherwise use everyone who appears in history
    if TEAM:
        # Ensure every team member has an entry (even if empty history)
        for member in TEAM:
            if member not in events:
                events[member] = []
        candidate_events = {p: evs for p, evs in events.items() if p in TEAM}
    else:
        candidate_events = dict(events)

    # Remove the PR author from scoring (they still appear in submitted events
    # for other people's score computation, but we don't assign to them)
    if not candidate_events:
        print("[main] No candidate reviewers found. Exiting without assignment.")
        sys.exit(0)

    # Add zero-entry for anyone in TEAM missing from history (new members)
    for p in list(candidate_events.keys()):
        if not candidate_events[p]:
            candidate_events[p] = [(T, 0.0, -1)]  # dummy event at now with 0 weight

    # Compute and display scores
    scores     = compute_scores(candidate_events, T)
    normalized = normalize(scores)

    print("\n[scores] Current load (normalized, lower = less loaded):")
    for p, s in sorted(normalized.items(), key=lambda x: x[1]):
        marker = " ← author (excluded)" if p == PR_AUTHOR else ""
        print(f"  {p:30s}  k'={s:10.1f}{marker}")

    # Select reviewer (exclude PR author)
    assignee = weighted_sample(normalized, exclude=PR_AUTHOR)
    print(f"\n[assign] Selected reviewer: {assignee}")

    # Assign on GitHub
    gh_post(
        f"/repos/{REPO}/pulls/{PR_NUMBER}/requested_reviewers",
        {"reviewers": [assignee]},
    )
    print(f"[assign] Successfully requested review from {assignee} on PR #{PR_NUMBER}.")
    # Remove the team review request so only the individual is shown
    team_slug = os.environ["REQUESTED_TEAM_SLUG"]
    gh_delete(
        f"/repos/{REPO}/pulls/{PR_NUMBER}/requested_reviewers",
        {"team_reviewers": [team_slug]},
    )


if __name__ == "__main__":
    main()
