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

**META1.** Everyone in the team can propose additional guidelines.<br>
**META2.** Everyone in the team can question and propose changes to guidelines.<br>
**META3.** Before the *first* set of guidelines is established, everyone in the team is invited to
participate in live discussions about them.<br>
**META4.** Before a new version of these guidelines is established, everyone in the team reviews it
independently, except if away on prolonged absence.<br>
**META5.** Ideally, all team members come to *agree* on any given version of these guidelines before
it is established.<br>
**META6.** Where that is not possible, preferably a majority of the team *agrees* with any given
version of these guidelines before it is established.<br>
**META7.** Preferably, all team members *accept* the latest established version of these guidelines,
until the team agrees to modify it.<br>
**META8.** In any case, all team members *abide* by the latest established version of these
guidelines, until the team agrees to modify it.<br>
**META9.** Established guidelines are taken seriously, but with a grain of salt. They are guidelines
after all, not absolute rules.<br>
**META10.** Once established, the first version of these guidelines is transferred to a versioned
document (CONTRIBUTING.md).<br>
**META11.** Going forward, these guidelines are meant to be expanded, refined, and corrected via
pull requests.<br>
**META12.** New practices can be experimented with before being adopted definitively.<br>

### Core principles (MU)

Principles for the members of the Multipass team. Many of these are inspired
by [Canonical's values](https://discourse.canonical.com/t/reaffirming-our-company-values/4525),
which we should keep in mind.

**MU1.** Aim at excellence.<br>
**MU2.**
Follow best practices (refer to other pertinent documents,
e.g. [CppCoreGuidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)).<br>
**MU3.** Think critically (even about best practices).<br>
**MU4.** Favor collaboration.<br>
**MU5.** Be open to feedback.<br>
**MU6.** Provide polite and sensitive feedback.<br>
**MU7.** Indulge your reviewer (if it's all the same to the project and yourself).<br>
**MU8.** If you believe you have it right, you have a right to defend why (arguably a duty).<br>
**MU9.** When it comes to that, recognize that hierarchy determines who has the final word.<br>
**MU10.** Be attentive and try to avoid mistakes.<br>
**MU11.** Don't agonize over mistakes (you'll inevitably make them).<br>
**MU12.** Be tolerant (nobody is perfect).<br>
**MU13.** Strive to improve (you always can).<br>
**MU14.** Leave the code a little bit better than you found it.<br>
**MU15.** Aim for optimal solutions when they are feasible, compromise when they are not.<br>

### Releases (REL)

Descriptive rules of how releases are obtained from Git.

**REL1.** The trunk of Multipass development happens in the `main` branch, which releases branch out
of.<br>
**REL2.** Preferably, release branches contain only commits that are directly reachable from `main`
or cherry-picked from it.<br>
**REL3.** Cherry-picked commits in release branches may differ from the original ones only where
necessary to avoid or fix conflicts.<br>
**REL4.** In exceptional cases, release branches may contain dedicated commits for bug, build, or
conflict fixes.<br>
**REL5.** After a release is published, the corresponding release branch is merged back into
`main`.<br>

### Pull Requests (PR)

Guidelines for how we use and handle pull requests.

**PR1.** Concrete modifications of Multipass can be proposed
via [Pull Requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests)
(AKA PRs) targeting the `main` branch.<br>
**PR2.** Prefer small, single-issue PRs.<br>
**PR3.** A PR should introduce a coherent change that appears as a unit in a medium or high level of
abstraction.<br>
**PR4.** The `main` branch is modified exclusively via PRs, except for an empty commit after
branching for release.<br>
**PR5.** PRs accepted into `main` are merged with merge commits.<br>
**PR6.** PRs to `main` should typically be covered by automated tests.<br>
**PR7.** If a PR is valuable on its own, does not depend on others, and does not involve dead code,
target the `main` branch, even if it is part of a larger task. This should be the most common
case.<br>
**PR8.** If your PR relies on another one, target the other's branch.<br>
**PR9.** When working on a larger set of changes with cohesive interdependence, consider using a
feature branch.<br>
**PR10.** Try to keep the number of concurrent feature branches small.<br>
**PR11.** When PRs are stacked, prefer to merge them in order. The target branch will update
automatically upon merging.<br>
**PR12.** PRs should include descriptions and/or point to appropriate context (within reason).<br>
**PR13.** When authoring a PR, make sure to test it.<br>
**PR14.** When authoring a PR, make sure to review its diff.<br>

### Reviews (RVW)

**RVW1.** Reviews can be distinguished as primary and secondary.<br>
**RVW2.** Primary reviews should approve PRs only after close inspection, impact consideration, and
appropriate testing.<br>
**RVW3.** Secondary reviews may approve PRs after a lighter overview of the changes.<br>
**RVW4.** A review is assumed to be primary, unless otherwise indicated (on GitHub or another
medium).<br>
**RVW5.** PRs may normally be merged only after two approvals: one primary and one secondary. This
is mandatory for external PRs (authored or committed from outside the Multipass team and the
Renovate bot).<br>
**RVW6.** After a PR is approved by multiple people, small updates require only a single additional
approval (i.e. after multiple approvals are dismissed).<br>
**RVW7.** Notably trivial PRs by the Multipass team may be merged after a single primary
approval.<br>
**RVW8.** Renovate PRs may be merged after a single primary approval.<br>
**RVW9.** Review comments should be acknowledged by the author, but *resolved* by the reviewer.<br>

### Versioning (GIT)

**GIT1.** Strive for atomic commits. A commit should introduce a coherent change that appears as a
unit in a low level of abstraction.<br>
**GIT2.** As a rule of thumb, commit messages of the form "do this and that" are an indication that
there should be two commits instead.<br>
**GIT3.** Strive to preserve a clean but detailed git history.<br>
**GIT4.** Avoid squashing.<br>
**GIT5.** Prefer additional commits during review (easier for reviewers to see the diff).<br>
**GIT6.** Avoid merging the target branch back into the topic branch. Rebase instead.<br>
**GIT7.** External contributors are encouraged to
[sign their commits](https://docs.github.com/en/authentication/managing-commit-signature-verification/signing-commits),
while Multipass team members are required to do so.<br>
**GIT8.** Use kebab-case branch names (i.e. lower-case-words-separated-with-hyphens).<br>
**GIT9.** Do not introduce whitespace errors.<br>

### Commit messages (MSG)

Guidelines for writing commit messages for non-merge commits. They are inspired by
[this](https://cbea.ms/git-commit/)
[and](https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html)
[other](https://preslav.me/2015/02/21/what-s-with-the-50-72-rule/)
[posts](https://stackoverflow.com/questions/2290016/git-commit-messages-50-72-formatting).
The category prefix is the main originality.

**MSG1.** Begin with a subject line.<br>
**MSG2.** Start the subject line with a lower-case, single-word category, within square brackets
(hyphenated, composite words are acceptable).<br>
**MSG3.** If you find yourself wanting multiple categories, consider splitting commits. Otherwise,
try to find a generic unifying category, or choose the most relevant.<br>
**MSG4.** Leave a single space after the category and capitalize the first ensuing word.<br>
**MSG5.** Limit the subject line to 50 characters (category included).<br>
**MSG6.** Do not end the subject line with a period.<br>
**MSG7.** Use the imperative mood in the subject line (e.g. "Fix bug" rather than "Fixed bug" or
"Fixes bug").<br>
**MSG8.** If adding a body, separate it from the subject with a blank line.<br>
**MSG9.** Use multiple paragraphs in the body if needed. Separate them with a blank line.<br>
**MSG10.** Do not include more than 1 consecutive blank line.<br>
**MSG11.** Use punctuation normally in the body.<br>
**MSG12.** Wrap the body at 72 characters.<br>
**MSG13.** Use the body to explain *what* and *why*, rather than *how*.<br>
**MSG14.** Be descriptive but succinct and avoid filler text.<br>
**MSG15.** Omit the body if the subject is self-explanatory.<br>
**MSG16.** Common abbreviations are fine (e.g. "msg" or "var").<br>

#### Examples

- Common categories: `[daemon]`, `[cli]`, `[settings]`, `[build]`, `[ci]`, `[test]`, `[doc]`,
  `[gui]`, `[format]`
- Subject line: `[gui]` Fix button alignment in navigation bar
- Full commit message with a body:

```
[completion] Avoid evaluating function input

Avoid calling `eval` with function arguments, to reduce the chance for
code injection (now or in the future).
```

#### Helper tools

- Consider setting your commit template to the `.gitmessage` file in the repository root:
    * `git config --local commit.template .gitmessage`
- Consider adding the `commit-msg` file in the repository root as a git hook (in `.git/hooks`)
    * `ln -s ../../git-hooks/commit-msg.py .git/hooks/commit-msg`

### Dependencies (DEP)

**DEP1.** Acceptable mechanisms to adopt source-code dependencies are, in decreasing order of
preference: Vcpkg (for C++) > FetchContent > submodule.<br>
**DEP2.** Avoid vendoring (copied source code).<br>

### Code

Prescriptive programming and engineering principles to aim at. Many of these overlap. Some are in
tension and have to be balanced.

#### Generic engineering and code (COD)

**COD1.** Avoid duplicate sources of truth.<br>
**COD2.** Prefer small units, be they functions, classes, files, etc.<br>
**COD3.** Follow [SOLID principles](https://en.wikipedia.org/wiki/SOLID).<br>
**COD4.** Pay special attention to the principles of
[single responsibility](https://en.wikipedia.org/wiki/Single-responsibility_principle) and
[separation of concerns](https://en.wikipedia.org/wiki/Separation_of_concerns).<br>
**COD5.** Don't repeat yourself ([DRY](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)).<br>
**COD6.** Don't reinvent the wheel.<br>
**COD7.** Don't
overengineer ([YAGNI](https://en.wikipedia.org/wiki/You_aren%27t_gonna_need_it)).<br>
**COD8.** Avoid coupling functionality that isn't logically coupled.<br>
**COD9.** Encapsulate, within each unit, all information that other units don't need.<br>
**COD10.** Encapsulate data with dependent behavior.<br>
**COD11.** Strive for interfaces that are easy to use, but hard to misuse.<br>
**COD12.** Avoid redundant levels of indirection.<br>
**COD13.** Simplify. Avoid pointless complexity.<br>
**COD14.** All else being equal, less code is better code.<br>
**COD15.** Don't try to maximize LoC metrics.<br>
**COD16.** Aim for consistency.<br>
**COD17.** If it is all the same otherwise, follow a single approach (see how it is done
elsewhere).<br>
**COD18.** Be creative. If you have a better approach, propose it and discuss it openly.<br>
**COD19.** Prioritize. Balance idealism with pragmatism.<br>
**COD20.** Document public, user-facing interfaces.<br>
**COD21.** Keep documentation up to date.<br>

#### C++ (CPP)

**CPP1.** Stick to standard C++17, with the exception of `#pragma once`.<br>
**CPP2.** Prefer `#pragma once` to header guards<br>
**CPP3.** Prefer enforcing correct usage (prevent misuse) at compilation time.<br>
**CPP4.** If a type isn't meant to be copied or moved, inherit from `DisabledCopyMove` (either
directly or indirectly).<br>
**CPP5.** For such types, define only constructors that fully initialize objects. Avoid a default
constructor unless the object needs no parameterization.<br>
**CPP6.** Make copyable
types [semiregular](https://en.cppreference.com/w/cpp/concepts/semiregular).<br>
**CPP7.** Avoid two-stage initialization. Initialize objects fully in the constructor.<br>
**CPP8.** Avoid const by-value params (e.g. no `void foo(const bool flag);`)<br>
**CPP9.** Encapsulate platform-dependent functionality in dedicated units (types, functions) and do
not use platform `#ifdef`s (or other platform-conditional logic) outside of those units.<br>
**CPP10.** Use `CamelCase` for types, but `snake_case` for variables and functions.<br>
**CPP11.** Avoid magic numbers.<br>
**CPP12.** Declare generic constants in a dedicated header.<br>
**CPP13.** Avoid compilation warnings.<br>
**CPP14.** To mock free functions and external APIs, wrap them with `MockableSingleton`.<br>

### Text

Prescriptive guidelines concerning text and written communication, including but not limited to:
PR descriptions, commit messages, comments, and documentation.

**TXT1.** Be clear, precise, informative, and organized.<br>
**TXT2.** Avoid filler text or fluff.<br>
**TXT3.** Avoid repetition.<br>
**TXT4.** Use references, quotes, citations, or links to provide context, substantiate claims,
and increase reliability.<br>
**TXT5.** Express the same idea more than once only to add relevant information, improve precision,
provide a different perspective, or help clarify complexity.<br>

### AI

Prescriptive guidelines concerning the use of Artificial Intelligence (AI) when contributing to
Multipass, in particular generative AI and LLMs.

**AI1.** Every contribution must be authored by human beings (one or more),
possibly with the help of AI tools. In addition to code contributions, this applies to issues, pull
requests, discussions, and any other comments or communications, regardless of the platform.<br>
**AI2.** Contributors are free to use any tools they see fit, including AI tools, provided they do
so lawfully and ethically.<br>
**AI3.** Git commits must have a human author, with a valid human-managed email address.<br>
**AI4.** The author(s) of a contribution must be able to explain the contribution in detail,
including any code or textual changes.<br>
**AI5.** Contributors must refrain from submitting code, text, or any other content that they don't
fully understand.<br>
**AI6.** Contributors must submit only modifications that they have reviewed thoroughly,
especially if it includes AI-generated content.<br>
**AI7.** When using AI, contributors should make an extra effort to ensure good information density
(i.e. high signal-to-noise ratio). This includes code and text.<br>
**AI8.** Authors are ultimately responsible for their entire contributions,
including all components, contents, format, presentation, and communication.<br>

# Further Information

- https://github.com/canonical/desktop-engineering/blob/main/project-repo/review-process.md
- https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/setting-guidelines-for-repository-contributors
- https://cbea.ms/git-commit/
- https://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html
- https://preslav.me/2015/02/21/what-s-with-the-50-72-rule/
- https://stackoverflow.com/questions/2290016/git-commit-messages-50-72-formatting
- Future work: https://pre-commit.com/
