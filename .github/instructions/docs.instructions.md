---
applyTo: "docs/**"
---

# Documentation

User-facing docs are Sphinx with the Canonical starter pack, written in MyST Markdown. Follow
`docs/contribute-to-multipass-docs.md` and the Canonical documentation style guide.

## Structure

- Place pages by Diátaxis type: `tutorial/`, `how-to-guides/`, `reference/`, `explanation/`. Keep
  the existing structure unless asked to reorganize it.
- Start every page with a label derived from its path, then the title:

  ```markdown
  (reference-command-line-interface-launch)=
  # launch
  ```

- Link with `{ref}` to a label or with root-relative paths such as
  `[find](/reference/command-line-interface/find)`. Add a `> See also:` line to related pages
  where the surrounding pages do.
- Register new pages in the parent `index.md` toctree (many use `:glob:`, so check before adding
  entries by hand).

## Keeping reference pages in sync

- CLI commands have one page each in `reference/command-line-interface/<command>.md`. When a
  command's options change, update the prose and the embedded `multipass help <command>` output.
- Settings have one page each in `reference/settings/`, named after the key with dots replaced by
  hyphens (`client.primary-name` becomes `client-primary-name.md`). Also add new settings to the
  hand-maintained list in `reference/settings/index.md`.
- Mark platform-specific steps with `{tab-set}` / `{tab-item}` using `:sync:` keys (`Linux`,
  `macOS`, `Windows`), as in `how-to-guides/customise-multipass/set-up-the-driver.md`.
- Use MyST admonitions (`{note}`, `{caution}`, `{seealso}`) and `{code-block}` for examples.

## Validation

Run from `docs/`. CI runs Canonical's shared documentation checks on PRs that touch `docs/`.

```bash
make html        # build with warnings as errors
make spelling    # add valid project terms to docs/.custom_wordlist.txt
make linkcheck
make lint-md
make woke
```

State which of these were run; `linkcheck` needs network access.
