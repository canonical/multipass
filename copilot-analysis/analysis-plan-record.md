# Analysis Plan Record

This file is an incrementally-updated log of decisions validated with the user
for the repository analysis of `canonical/multipass`. Each entry below is only
added once explicitly confirmed, so this file can be used to resume work if
the session is interrupted.

## Status: Planning complete — ready to begin Stage 0 (Architecture overview)

## Working Style (validated)

- Interview/discussion happens as normal chat back-and-forth (free text),
  not structured forms.
- Structured yes/no prompts are reserved only for authorization requests
  (e.g., permission to create/edit files, run risky commands).

## Validated Steps

1. **Authorization:** User authorized Copilot CLI to create/edit files under
   `multipass/copilot-analysis/` freely for this session (record file,
   diagrams, reports, etc.) without asking each time.
2. **Goal:** User is a new joiner to the team. Primary goal is twofold:
   - Get a high-level understanding of the project, especially its
     **architecture**.
   - Additionally dive into a set of **specific topics** (see "Analysis
     Topics" section below).

## Analysis Topics (validated, list kept open for additions)

### High-level
- Architecture & module structure overview (primary/first deliverable)

### Deep-dive topics
1. Build system & CI/CD (CMake, vcpkg, snapcraft, GitHub Actions)
2. Platform-specific backends (QEMU/Linux, Hyperkit-VZ/macOS, Hyper-V/Windows, LXD)
3. Daemon <-> client architecture & gRPC protocol
4. Networking implementation (bridges, DNS, port forwarding)
5. Image/instance lifecycle management
6. Mount implementation (native/sshfs)
7. Testing strategy (unit/mock/integration test structure)
8. Dependency management (third-party libs via vcpkg)
9. Release process & versioning
10. Current pain-points and tech-debt — includes code-level analysis AND
    mining GitHub issues (open bugs, long-standing issues, discussions) to
    identify known problem areas
11. Development workflow — how to build, debug, test locally, contributor
    tooling
12. Specific features — e.g. availability zones, aliases, and other notable
    CLI features (list to be refined as we go)
13. The GUI (multipass-gui or equivalent client)
14. Platform-specific divergences — behavioral differences between
    Windows/Linux/macOS in parts of the app that are meant to behave
    consistently
15. Entities & relationships — key objects/classes, ownership, and lifespan
    (object lifecycle map)
16. Threading model

> Note: this topic list is intentionally open — more topics may be added
> later as they come up.

## Deferred Decisions

- **Depth per topic:** Not decided upfront for the whole plan. When we begin
  analyzing a given topic, ask the user at that time how deep to go for that
  specific topic (quick summary vs. moderate detail vs. deep-dive with file
  references).

## Output Format (validated)

- **One markdown document per topic**, stored under `copilot-analysis/`.
- Some currently-listed items may actually cover several distinct topics
  (e.g. "platform-specific backends" could split into per-backend docs, or
  "specific features" could split per feature). When starting work on such
  an item, split it into separate documents/topics as appropriate rather
  than forcing everything into one file.
- Naming convention and exact file list to be refined as topics are split
  and tackled.

## Execution Order (validated)

Staged, batch-based execution: architecture overview first (solo/foundational),
then remaining topics in dependency-ordered batches, each batch parallelized
across one agent per topic. Later batches can use earlier batches' docs as
context input, keeping per-agent context small.

- **Stage 0 — Foundational, solo, first:**
  - Architecture & module structure overview
- **Batch 1 — Foundational, parallel:**
  - Entities & relationships
  - Threading model
- **Batch 2 — Functional/domain, parallel:**
  - Platform-specific backends
  - Daemon <-> client architecture & gRPC protocol
  - Networking implementation
  - Image/instance lifecycle management
  - Mount implementation
  - Specific features (availability zones, aliases, etc.)
  - The GUI
  - Platform-specific divergences
- **Batch 3 — Process/meta, parallel:**
  - Build system & CI/CD
  - Testing strategy
  - Dependency management
  - Development workflow
  - Release process & versioning
- **Last — depends on everything above:**
  - Current pain-points and tech-debt (uses all prior docs + GitHub issues
    as context)

> Note: batches are kept open. New topics added later should be slotted into
> the batch matching their dependency level (foundational / functional /
> process / final cross-cutting), rather than always appended at the end.

## Open Questions

_(none currently open — see Deferred Decisions for items intentionally left
for later, and the topic list which remains open for additions)_

## GitHub Issue Mining (validated)

- For the "Current pain-points and tech-debt" topic, GitHub tools are
  authorized to query/search issues (and related discussions) on
  `canonical/multipass` to help identify known problem areas, in addition to
  code-level analysis.
