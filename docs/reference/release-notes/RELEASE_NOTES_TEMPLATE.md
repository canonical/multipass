---
orphan: true
---

({{VERSION_ANCHOR}})=
# {{VERSION}}

{{BRIEF_DESCRIPTION}}

{{#BREAKING_CHANGES}}
## Breaking Changes

{{#breaking_changes.items}}
- [{{category}}] {{subject}} ([#{{pr_number}}](https://github.com/canonical/multipass/pull/{{pr_number}}))
{{/breaking_changes.items}}

{{/BREAKING_CHANGES}}
## Features

{{#features.by_category}}
{{#items}}
- [{{category}}] {{subject}} ([#{{pr_number}}](https://github.com/canonical/multipass/pull/{{pr_number}}))
{{/items}}

{{/features.by_category}}

## Bug fixes

{{#fixes.by_category}}
{{#items}}
- [{{category}}] {{subject}} ([#{{pr_number}}](https://github.com/canonical/multipass/pull/{{pr_number}}))
{{/items}}

{{/fixes.by_category}}

{{#DEPRECATIONS}}
## Deprecations

{{#deprecations}}
- {{.}}
{{/deprecations}}

{{/DEPRECATIONS}}
{{#DOCS}}
## Documentation

{{#docs.items}}
- {{subject}} ([#{{pr_number}}](https://github.com/canonical/multipass/pull/{{pr_number}}))
{{/docs.items}}

{{/DOCS}}
## New contributors

Special thanks to our new contributors

{{#new_authors.names}}
- [@{{.}}](https://github.com/{{.}})
{{/new_authors.names}}

## Changes

See also the full diff: [`{{PREVIOUS_TAG}}...{{VERSION}}`](https://github.com/canonical/multipass/compare/{{PREVIOUS_TAG}}...{{VERSION}}).

## Feedback

Please file issues on the [Multipass GitHub](https://github.com/canonical/multipass/issues/new/choose) for problems and feature requests, or join the [Multipass Discourse forum](https://discourse.ubuntu.com/c/project/multipass) to chat. You can also find us in the [Multipass room on Matrix](https://matrix.to/#/#multipass:ubuntu.com).
