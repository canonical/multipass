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
    padding-left: 60px;
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

<ol class="custom-list" style="--prefix: 'MU';">
<li>Aim at excellence.</li>

<li>Follow best practices (refer to other pertinent documents, e.g.CppCoreGuidelines).</li>

<li>Think critically (even about best practices).</li>

<li>Favor collaboration.</li>

<li>Be open to feedback.</li>

<li>Provide polite and sensitive feedback.</li>

<li>Indulge your reviewer (if it's all the same to the project and yourself).</li>

<li>If you believe you have it right, you have a right to defend why (arguably a duty).</li>

<li>When it comes to that, recognize that hierarchy determines who has the final word.</li>

<li>Be attentive and try to avoid mistakes.</li>

<li>Don't agonize over mistakes (you'll inevitably make them).</li>

<li>Be tolerant (nobody is perfect).</li>

<li>Strive to improve (you always can).</li>

<li>Leave the code a little bit better than you found it.</li>

<li>Aim for optimal solutions when they are feasible, compromise when they are not.</li>
</ol>

### Releases (REL)

Descriptive rules of how releases are obtained from Git.

<ol class="custom-list" style="--prefix: 'REL';">
<li>The trunk of Multipass development happens in the main branch, which releases branch out
of.</li>

<li>Preferably, release branches contain only commits that are directly reachable from main or
cherry-picked from it.</li>

<li>Cherry-picked commits in release branches may differ from the original ones only where
necessary to avoid or fix conflicts.</li>

<li>In exceptional cases, release branches may contain dedicated commits for bug, build, or
conflict fixes.</li>

<li>After a release is published, the corresponding release branch is merged back into main.</li>
</ol>

### Pull Requests (PR)

Guidelines for how we use and handle pull requests.

<ol class="custom-list" style="--prefix: 'PR';">
<li>Concrete modifications of Multipass can be proposed via Pull Requests (AKA PRs) targeting
the main branch.</li>

<li>Prefer small, single issue PRs.</li>

<li>A PR should introduce a coherent change that appears as a unit in a medium or high level of
abstraction.</li>

<li>The main branch is modified exclusively via PRs, except for an empty commit after branching
for release.</li>

<li>PRs accepted into main are merged with merge commits.</li>

<li>PRs to main should typically be covered by automated tests.</li>

<li>If a PR is valuable on its own, does not depend on others, and does not involve dead code,
target the main branch, even if it is part of a larger task. This should be the most common case.
</li>

<li>If your PR relies on another one, target the other's branch.</li>

<li>When working on a larger set of changes with cohesive interdependence, consider using a
feature branch.</li>

<li>Try to keep the number of concurrent feature branches small.</li>

<li>When PRs are stacked, prefer to merge them in order. The target branch will update
automatically upon merging.</li>

<li>PRs should include descriptions and/or point to appropriate context (within reason).</li>

<li>When authoring a PR, make sure to test it.</li>

<li>When authoring a PR, make sure to review its diff.</li>
</ol>

### Reviews (RVW)

<ol class="custom-list" style="--prefix: 'RVW';">
<li>Reviews can be distinguished as primary and secondary.</li>

<li>Primary reviews should approve PRs only after close inspection, impact consideration, and
appropriate testing.</li>

<li>Secondary reviews may approve PRs after a lighter overview of the changes.</li>

<li>A review is assumed to be primary, unless otherwise indicated (on GitHub or another
medium).</li>

<li>PRs may normally be merged only after two approvals: one primary and one secondary. This
is mandatory for external PRs (authored or committed from outside the Multipass team and the
Renovate bot).</li>

<li>After a PR is approved by multiple people, small updates require only a single additional
approval (i.e. after multiple approvals are dismissed).</li>

<li>Notably trivial PRs by the Multipass team may be merged after a single primary approval.</li>

<li>Renovate PRs may be merged after a single primary approval.</li>

<li>Review comments should be acknowledged by the author, but resolved by the reviewer.</li>
</ol>

### Versioning (GIT)

<ol class="custom-list" style="--prefix: 'GIT';">
<li>Strive for atomic commits. A commit should introduce a coherent change that appears as a
unit in a low level of abstraction.</li>

<li>As a rule of thumb, commit messages of the form "do this and that" are an indication that
there should be two commits instead.</li>

<li>Strive to preserve a clean but detailed git history.</li>

<li>Avoid squashing.</li>

<li>Prefer additional commits during review (easier for reviewers to see the diff).</li>

<li>Avoid merging the target branch back into topic. Rebase instead.</li>

<li>External contributors are encouraged to sign their commits, while Multipass team members
are required to do so.</li>

<li>Use kebab-case branch names (i.e. lower-case-words-separated-with-hyphens).</li>

<li>Do not introduce whitespace errors.</li>
</ol>

## Unset

### Commit messages (MSG)

Guidelines for writing commit messages for non-merge commits. They are inspired by this and other
posts. The category prefix is the main originality.

<ol class="custom-list" style="--prefix: 'MSG';">
<li>Begin with a subject line.</li>

<li>Start the subject line with a lower-case, single-word category, within square brackets (
hyphenated, composite words are acceptable).</li>

<li>If you find yourself wanting multiple categories, consider splitting commits. Otherwise,
try to find a generic unifying category, or choose the most relevant.</li>

<li>Leave a single space after the category and capitalize the first ensuing word.</li>

<li>Limit the subject line to 50 characters (category included).</li>

<li>Do not end the subject line with a period.</li>

<li>Use the imperative mood in the subject line (e.g. "Fix bug" rather than "Fixed bug" or "
Fixes bug").</li>

<li>If adding a body, separate it from the subject with a blank line.</li>

<li>Use multiple paragraphs in the body if needed. Separate them with a blank line.</li>

<li>Do not include more than 1 consecutive blank line.</li>

<li>Use punctuation normally in the body.</li>

<li>Wrap the body at 72 characters.</li>

<li>Use the body to explain what and why, rather than how.</li>

<li>Be descriptive but succinct and avoid filler text.</li>

<li>Omit the body if the subject is self-explanatory.</li>

<li>Common abbreviations are fine (e.g. "msg" or "var")</li>

</ol>

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

<ol class="custom-list" style="--prefix: 'DEP';">
<li>Acceptable mechanisms to adopt source-code dependencies are, in decreasing order of
preference: vcpkg (for C++) > FetchContent > submodule.</li>

<li>**DEP2.** Avoid vendoring (copied source code).</li>
</ol>

## Code

Prescriptive programming and engineering principles to aim at. Many of these overlap. Some are in
tension and have to be balanced.

### Generic engineering and code (COD)

<ol class="custom-list" style="--prefix: 'COD';">
<li>Avoid duplicate sources of truth.</li>

<li>Prefer small units, be they functions, classes, files, etc.</li>

<li>Follow SOLID principles.</li>

<li>Pay special attention to the principles of single responsibility and separation of concerns.
</li>

<li>Don't repeat yourself (DRY).</li>

<li>Don't reinvent the wheel.</li>

<li>Don't over-engineer (YAGNI).</li>

<li>Avoid coupling functionality that isn't logically coupled.</li>

<li>Encapsulate, within each unit, all information that other units don't need.</li>

<li>Encapsulate data with dependent behavior.</li>

<li>Strive for interfaces that are easy to use, but hard to misuse.</li>

<li>Avoid redundant levels of indirection.</li>

<li>Simplify. Avoid pointless complexity.</li>

<li>All else being equal, less code is better code.</li>

<li>Don't try to maximize LoC metrics.</li>

<li>Aim for consistency.</li>

<li>If it is all the same otherwise, follow a single approach (see how it is done elsewhere).</li>

<li>Be creative. If you have a better approach, propose it and discuss it openly.</li>

<li>Prioritize. Balance idealism with pragmatism.</li>

<li>Document public, user-facing interfaces.</li>

<li>Keep documentation up-to-date.</li>
</ol>

### C++ (CPP)

<ol class="custom-list" style="--prefix: 'CPP';">
<li>Stick to standard C++17, with the exception of #pragma once. </li>

<li>Prefer "#pragma once" to header guards</li>

<li>Prefer enforcing correct usage (prevent misuse) at compilation time.</li>

<li>If a type isn't meant to be copied or moved, inherit from DisabledCopyMove (either
directly or indirectly).</li>

<li>For such types, define only constructors that fully initialize objects. Avoid a default
constructor unless the object needs no parameterization.</li>

<li>Make copyable types semi-regular.</li>

<li>Avoid two-stage initialization. Initialize objects fully in the constructor.</li>

<li>Avoid const by-value params (e.g. no void foo(const bool flag);)</li>

<li>Encapsulate platform-dependent functionality in dedicated units (types, functions) and do
not use platform #ifdefs (or other platform-conditional logic) outside of those units.</li>

<li>Use CamelCase for types, but snake_case for variables and functions.</li>

<li>Avoid magic numbers.</li>

<li>Declare generic constants in a dedicated header.</li>

<li>Avoid compilation warnings.</li>

<li>To mock free functions and external APIs, wrap them with MockableSingleton</li>

</ol>
