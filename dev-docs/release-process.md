# Instructions for releasing a new version of Multipass

The procedure for [feature releases (minor or major)](#feature-releases-minormajor) is slightly different from [patch/maintenance releases](#patch-releases).

Shell commands have the prompt format `branchname` to clarify the context in which the git command is executed e.g. `main`, `release/1.15` etc.
In this example, we want to release version 1.15.0.

## Feature releases (minor/major)

### Prepare the ground for a feature release

1. Merge the `stable-docs` branch onto `main`. This will bring any diverging changes to documentation onto the future release branch.
2. Create a release branch and tag it for RC. Keep in mind, RCs are now versioned e.g. `rc1`.

```shell
main $ git switch --create release/1.15
release/1.15 $ git push --set-upstream origin HEAD
release/1.15 $ git tag v1.15.0-rc1
release/1.15 $ git push --tags
```
3. Create development commit for the next version and tag it.
```shell
main $ git commit --allow-empty --message 'Begin 1.16.0 development'
main $ git push
main $ git tag v1.16.0-dev
main $ git push --tags
```
4. Publish the RC on GitHub.

## Hotfixes

After creating the RC, if there are hotfixes that need to make their way into a new RC, those hotfixes must first be merged into `main` and then the merge commits must be cherry-picked on top of the release branch. For example:

```shell
main $ git switch --create hotfix
# Do hotfix work.
hotfix $ git commit --message 'implemented hotfix'
hotfix $ git push --set-upstream origin HEAD
# Create a PR and merge into main when done.
main $ git pull
# Identify the merge commit(s) of the PRs that should be added to the release
# For each commit:
main $ git log # copy the hash of the hotfix merge commit
release/1.15 $ git cherry-pick --mainline 1 <hotfix-merge-commit-hash>
release/1.15 $ git push
```
Repeat this process until we are happy with the hotfixes and we want to release a new RC.

## Patch releases

When starting a new patch release, the `stable-docs` branch doesn't need to be independently merged into main. The new patch release will include it and merge into main eventually.

Patch releases are very similar to hotfixes but differ in tags for RCs.

* We keep using the `release/1.15` branch.
* We keep cherry-picking commits from `main` on top of the appropriate `release/1.15` branch.
* When we want to release an RC for a patch version, we must tag it as `v1.15.1-rc1`, `v1.15.1-rc2` etc. until we make the actual release of the patch version with the signed tag `v1.15.1`.

## New Release Candidate

Tag the new RC.
```shell
release/1.15 $ git tag v1.15.0-rc2
release/1.15 $ git push --tags
```
After this, publish the new RC again.

## The final release

After we are happy with the state of the latest RC, we can turn them into an actual release.

### Prepare the release

1. Add a signed tag on the same commit as the RC tag that you want to release.
```shell
release/1.15 $ git tag --sign v1.15.0 --message 'Multipass version 1.15.0'
release/1.15 $ git push --tags
```
2. Restart the GHA run that got triggered by the release branch (`release/1.15` in this example), so that it picks up the new tag when deriving the version.
3. Get win/mac packages from the run that is triggered by the _release branch_ (not the tag where the version is incorrect ATTOW).
    * Send the newly generated packages to IS for signing, likely using a Concordia ticket at https://concordia.canonical.com/tickets
4. Move the final launchpad build into the beta channel, in https://snapcraft.io/multipass/releases
    * First kick the build in launchpad if necessary.
    * Be sure to select a launchpad build and not a GH one. Launchpad builds should show with an "lp" suffix on that page, e.g. "1.15.0 | lp-91234567". The suffix is not part of the actual version (as confirmed by `snap info` and `multipass version`).
5. When the signed packages are received from IS, verify their signature:
    * on macos: `pkgutil --check-signature <pkg>`
    * on Windows: right-click every `.exe`, select "Properties", "Digital Signatures" pane (or use [SignTool](https://learn.microsoft.com/en-us/windows/win32/seccrypto/using-signtool-to-verify-a-file-signature))
6. Create a draft release entry on GitHub.
    * attach the signed macOS and Windows packages.
7. Upload the package to the Microsoft Store.
    * Place the package in a public place, accessible with a no-redirect URL. For example https://people.canonical.com/~ricab/multipass-1.15.0+win-win64.msi.
    * Go to https://partner.microsoft.com/en-us/dashboard/apps-and-games/overview, navigate to "Multipass > Packages" and upload the package.
    * Run validation on the new package. This might take a few working days.
8. Submit a draft PR to the website to update the latest-release.json
9. Prepare release notes and release announcements for Discourse, Matrix, and Mattermost
10. Submit a PR to `main` with
    * The new release notes for this version in `docs/reference/release-notes`.
    * Update the `index.md` file as well, adding a link to the release notes and changing/adding details about the release's contents.
    * Follow the [template](https://github.com/canonical/multipass/blob/main/docs/reference/release-notes/release-notes-templates.md) and use these release notes in the GH draft release as well.

### Publicly release

1. Promote the snap from beta to stable.
2. Publish the release draft on GH
3. Submit the update on the Microsoft Store
4. Undraft the PR on the website (mark "ready for review")
    * Verify that the package links work after the release is published.
    * Follow up on the PR until it is merged.
5. Set the `stable` branch to point to the release. This will allow Launchpad to generate updated snaps in the candidate channel (with updated deb dependencies): Launchpad checks daily if there are out of date dependencies in our snap; if there are, a new package gets built as candidate and if it's good we promote it to stable.
```shell
stable $ git reset --hard release/1.15
stable $ git push --force
```
6. Merge the release notes documentation PR into `main`.
7. Fast-forward the `stable-docs` branch to point to the release. This corresponds to the stable version shown in ReadTheDocs. Since the `stable-docs` were merged into `main`, there should be no conflicts. Then cherry-pick the release notes PR on the `stable-docs` branch.
8. Merge the release branch into main. This is to keep track of the number of commits from one release to another.
```shell
main $ git merge release/1.15
main $ git push
```
9. Make sure that the release branch (`release/1.15` in this example) remains in the remote. If there is a corresponding PR, GitHub is likely to automatically delete it. If this is the case, restore it. Our candidate build on launchpad relies on this to figure out the correct version.
10. Announce the new release on Discourse, Matrix and Mattermost.

Steps to follow up on the following few days:

1. Verify that the Microsoft store submission is approved (moves from "in review")
2. Confirm that the PR for the latest version is merged into canonical.com
3. Confirm that candidate snaps are built for the release

## Note on `stable-docs` and release branches

`stable-docs` is supposed to always be on par or ahead in commit history, while sharing said commit history. This is because docs are generated every 24h from this branch, and it has to contain the historical changes plus documentation changes. This framework reduces the number of possible conflicts between documentation and source while keeping the maintenance cost to a minimum.

## Additional resources

[2024 Discourse announcement on the release process](https://discourse.ubuntu.com/t/multipass-release-process/10363)
