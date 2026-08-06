(For more generic notes on the release process, go [here](https://discourse.ubuntu.com/t/multipass-release-process/10363).)

1. Update the public release branch with intended changes:
    1. Checkout a local copy of the release branch (e.g. `git co release/1.11`).
    1. Identify the merge commits of the PRs that should be added to the release.
    1. Cherry pick each selected commit: `git cherry-pick --mainline=1 <commit-hash>`.
1. Update the snap's beta channel:
    1. Push to public remote; this triggers CI on GH, but the snap beta channel is obtained from launchpad.
    1. [Follow it](https://github.com/canonical/multipass/actions) and restart as needed (look for the one with the release branch).
    1. Launchpad eventually picks up the snap update and builds. Good idea to [keep an eye on it](https://launchpad.net/~multipass-team/multipass/+snap/multipass-beta) too. It can also be manually triggered.
1. Update the macOS and Windows installers on the RC page:
    1. Push to private remote. This triggers CI on the private side.
    1. Again, follow it and restart as needed. When done, download the artifacts.
    1. Edit the RC page (on the public GH repo):
        1. Update the release notes with the PRs that were cherry picked
        1. Upload and replace existing artifacts.
        1. Save without publishing (ATTOW, just need to make sure not to tick the "latest release" checkbox).