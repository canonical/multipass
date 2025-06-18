# Contribution guidelines for Multipass

## Rationale

Rowing a boat is more effective if everyone pulls in the same direction. The same idea applies to
software projects. Shared goals, principles, and procedures result in less friction and lead to more
consistent code, documentation, and user experiences.

At the same time, people have legitimate preferences, habits, and approaches that work well for
them. They need a high degree of freedom to be creative and effective. Furthermore, the world
changes and practices cannot all be anticipated at infinite resolution. It is thus clear that not
every aspect of software engineering needs to be governed by pre-established rules.

It's good that we're humans, not machines: in comparison, we excel in adaptation. Too fine a
rulebook would stifle the best we have to give, but we can still tune in on shared goals and
protocols to streamline work, maximize harmony, and assimilate newcomers.

Accordingly, this document aims at establishing a set of guidelines that:

- balance shared direction with autonomy;
- match shared sentiment in the Multipass team;
- afford a few grains of salt;
- are meant to evolve.

## Guidelines

In the lists that follow, prefixes are meant to provide an easy means of referring to individual
guidelines and sections. Order may follow a loose line of reasoning, but it does not signify
precedence or priority.

### Meta-guidelines (META)

The following meta-guidelines cover the process to derive Multipass's contribution guidelines, along
with governance goals.

<style>
.custom-list {
    counter-reset: item-counter;
    list-style: none;
    padding-left: 0;
}
.custom-list li {
    counter-increment: item-counter;
    position: relative;
    margin-left: 20px;
    padding-left: var(--label-width, 60px);
}
.custom-list li::before {
    content: var(--prefix, "ITEM") counter(item-counter) ". ";
    font-weight: bold;
    position: absolute;
    left: 0;
    top: 0;
}
</style>

<ol class="custom-list" style="--prefix: 'META';">
<li>Everyone in the team can propose additional guidelines.</li>

<li>Everyone in the team can question and propose changes to guidelines.</li>

<li>Before the first set of guidelines is established, everyone in the team is invited to
participate in live discussions about them.</li>

<li>Before a new version of these guidelines is established, everyone in the team reviews it
independently, except if away on prolonged absence.</li>

<li>Ideally, all team members come to agree on any given version of these guidelines before
it is established.</li>

<li>Where that is not possible, preferably a majority of the team agrees with any given
version of these guidelines before it is established.</li>

<li>Preferably, all team members accept the latest established version of these guidelines,
until the team agrees to modify it.</li>

<li>In any case, all team members abide by the latest established version of these
guidelines, until the team agrees to modify it.</li>

<li>Established guidelines are taken seriously, but with a grain of salt. They are guidelines
after all, not absolute rules.</li>

<li>Once established, the first version of these guidelines is transferred to a versioned
document (CONTRIBUTING.md).</li>

<li>Going forward, these guidelines are meant to be expanded, refined, and corrected via
pull requests.</li>

<li>New practices can be experimented with before being adopted definitively.</li>
</ol>

### Core principles (MU)

Principles for the members of the Multipass team. Many of these are inspired by Canonical's values,
which we should keep in mind.

**MU1.** Aim at excellence.

**MU2.** Follow best practices (refer to other pertinent documents, e.g.CppCoreGuidelines).

**MU3.** Think critically (even about best practices).

**MU4.** Favor collaboration.

**MU5.** Be open to feedback.

**MU6.** Provide polite and sensitive feedback.

**MU7.** Indulge your reviewer (if it's all the same to the project and yourself).

**MU8.** If you believe you have it right, you have a right to defend why (arguably a duty).

**MU9.** When it comes to that, recognize that hierarchy determines who has the final word.

**MU10.** Be attentive and try to avoid mistakes.

**MU11.** Don't agonize over mistakes (you'll inevitably make them).

**MU12.** Be tolerant (nobody is perfect).

**MU13.** Strive to improve (you always can).

**MU14.** Leave the code a little bit better than you found it.

**MU15.** Aim for optimal solutions when they are feasible, compromise when they are not.

### Releases (REL)

Descriptive rules of how releases are obtained from Git.

**REL1.** The trunk of Multipass development happens in the main branch, which releases branch out
of.

**REL2.** Preferably, release branches contain only commits that are directly reachable from main or
cherry-picked from it.

**REL3.** Cherry-picked commits in release branches may differ from the original ones only where
necessary to avoid or fix conflicts.

**REL4.** In exceptional cases, release branches may contain dedicated commits for bug, build, or
conflict fixes.

**REL5.** After a release is published, the corresponding release branch is merged back into main.

### Pull Requests (PR)

Guidelines for how we use and handle pull requests.

**PR1.** Concrete modifications of Multipass can be proposed via Pull Requests (AKA PRs) targeting
the main branch.

**PR2.** Prefer small, single issue PRs.

**PR3.** A PR should introduce a coherent change that appears as a unit in a medium or high level of
abstraction.

**PR4.** The main branch is modified exclusively via PRs, except for an empty commit after branching
for release.

**PR5.** PRs accepted into main are merged with merge commits.

**PR6.** PRs to main should typically be covered by automated tests.

**PR7.** If a PR is valuable on its own, does not depend on others, and does not involve dead code,
target the main branch, even if it is part of a larger task. This should be the most common case.

**PR8.** If your PR relies on another one, target the other's branch.

**PR9.** When working on a larger set of changes with cohesive interdependence, consider using a
feature branch.

**PR10.** Try to keep the number of concurrent feature branches small.

**PR11.** When PRs are stacked, prefer to merge them in order. The target branch will update
automatically upon merging.

**PR12.** PRs should include descriptions and/or point to appropriate context (within reason).

**PR13.** When authoring a PR, make sure to test it.

**PR14.** When authoring a PR, make sure to review its diff.

### Reviews (RVW)

**RVW1.** Reviews can be distinguished as primary and secondary.

**RVW2.** Primary reviews should approve PRs only after close inspection, impact consideration, and
appropriate testing.

**RVW3.** Secondary reviews may approve PRs after a lighter overview of the changes.

**RVW4.** A review is assumed to be primary, unless otherwise indicated (on GitHub or another
medium).

**RVW5.** PRs may normally be merged only after two approvals: one primary and one secondary. This
is mandatory for external PRs (authored or committed from outside the Multipass team and the
Renovate bot).

**RVW6.** After a PR is approved by multiple people, small updates require only a single additional
approval (i.e. after multiple approvals are dismissed).

**RVW7.** Notably trivial PRs by the Multipass team may be merged after a single primary approval.

**RVW8.** Renovate PRs may be merged after a single primary approval.

**RVW9.** Review comments should be acknowledged by the author, but resolved by the reviewer.

### Versioning (GIT)

**GIT1.** Strive for atomic commits. A commit should introduce a coherent change that appears as a
unit in a low level of abstraction.

**GIT2.** As a rule of thumb, commit messages of the form "do this and that" are an indication that
there should be two commits instead.

**GIT3.** Strive to preserve a clean but detailed git history.

**GIT4.** Avoid squashing.

**GIT5.** Prefer additional commits during review (easier for reviewers to see the diff).

**GIT6.** Avoid merging the target branch back into topic. Rebase instead.

**GIT7.** External contributors are encouraged to sign their commits, while Multipass team members
are required to do so.

**GIT8.** Use kebab-case branch names (i.e. lower-case-words-separated-with-hyphens).

**GIT9.** Do not introduce whitespace errors.

## Unset

### Commit messages (MSG)

Guidelines for writing commit messages for non-merge commits. They are inspired by this and other
posts. The category prefix is the main originality.

**MSG1.** Begin with a subject line.

**MSG2.** Start the subject line with a lower-case, single-word category, within square brackets (
hyphenated, composite words are acceptable).

**MSG3.** If you find yourself wanting multiple categories, consider splitting commits. Otherwise,
try to find a generic unifying category, or choose the most relevant.

**MSG4.** Leave a single space after the category and capitalize the first ensuing word.

**MSG5.** Limit the subject line to 50 characters (category included).

**MSG6.** Do not end the subject line with a period.

**MSG7.** Use the imperative mood in the subject line (e.g. "Fix bug" rather than "Fixed bug" or "
Fixes bug").

**MSG8.** If adding a body, separate it from the subject with a blank line.

**MSG9.** Use multiple paragraphs in the body if needed. Separate them with a blank line.

**MSG10.** Do not include more than 1 consecutive blank line.

**MSG11.** Use punctuation normally in the body.

**MSG12.** Wrap the body at 72 characters.

**MSG13.** Use the body to explain what and why, rather than how.

**MSG14.** Be descriptive but succinct and avoid filler text.

**MSG15.** Omit the body if the subject is self-explanatory.

**MSG16.** Common abbreviations are fine (e.g. "msg" or "var")

#### Examples

- Common categories: [daemon], [cli], [settings], [build], [ci], [test], [doc], [gui], [format]
- Subject line: [gui] Fix button alignment in navigation bar
- Full commit message with a body:

```
[completion] Avoid evaluating function input

Avoid calling `eval` with function arguments, to reduce the chance for
code injection (now or in the future).
```

#### Helper tools

- Consider setting your commit template to the .gitmessage file in the repository root.
- Consider adding the commit-msg file in the repository root as a git hook (in .git/hooks)

### Dependencies (DEP)

**DEP1.** Acceptable mechanisms to adopt source-code dependencies are, in decreasing order of
preference: vcpkg (for C++) > FetchContent > submodule.

**DEP2.** Avoid vendoring (copied source code).

## Code

Prescriptive programming and engineering principles to aim at. Many of these overlap. Some are in
tension and have to be balanced.

### Generic engineering and code (COD)

**COD1.** Avoid duplicate sources of truth.

**COD2.** Prefer small units, be they functions, classes, files, etc.

**COD3.** Follow SOLID principles.

**COD4.** Pay special attention to the principles of single responsibility and separation of
concerns.

**COD5.** Don't repeat yourself (DRY).

**COD6.** Don't reinvent the wheel.

**COD7.** Don't over-engineer (YAGNI).

**COD8.** Avoid coupling functionality that isn't logically coupled.

**COD9.** Encapsulate, within each unit, all information that other units don't need.

**COD10.** Encapsulate data with dependent behavior.

**COD11.** Strive for interfaces that are easy to use, but hard to misuse.

**COD12.** Avoid redundant levels of indirection.

**COD13.** Simplify. Avoid pointless complexity.

**COD14.** All else being equal, less code is better code.

**COD15.** Don't try to maximize LoC metrics.

**COD16.** Aim for consistency.

**COD17.** If it is all the same otherwise, follow a single approach (see how it is done elsewhere).

**COD18.** Be creative. If you have a better approach, propose it and discuss it openly.

**COD19.** Prioritize. Balance idealism with pragmatism.

**COD20.** Document public, user-facing interfaces.

**COD21.** Keep documentation up-to-date.

### C++ (CPP)

**CPP1.** Stick to standard C++17, with the exception of #pragma once.

**CPP2.** Prefer "#pragma once" to header guards

**CPP3.** Prefer enforcing correct usage (prevent misuse) at compilation time.

**CPP4.** If a type isn't meant to be copied or moved, inherit from DisabledCopyMove (either
directly or indirectly).

**CPP5.** For such types, define only constructors that fully initialize objects. Avoid a default
constructor unless the object needs no parameterization.

**CPP6.** Make copyable types semi-regular.

**CPP7.** Avoid two-stage initialization. Initialize objects fully in the constructor.

**CPP8.** Avoid const by-value params (e.g. no void foo(const bool flag);)

**CPP9.** Encapsulate platform-dependent functionality in dedicated units (types, functions) and do
not use platform #ifdefs (or other platform-conditional logic) outside of those units.

**CPP10.** Use CamelCase for types, but snake_case for variables and functions.

**CPP11.** Avoid magic numbers.

**CPP12.** Declare generic constants in a dedicated header.

**CPP13.** Avoid compilation warnings.

**CPP14.** To mock free functions and external APIs, wrap them with MockableSingleton
