# PR packages

Our CI produces packages that can be used to test PRs:

- On Linux: `snap refresh --channel edge/prXXXX`
  * replace `XXXX` with the PR number
  * replace `refresh` with `install` to install from scratch
  * warning: if the PR channel doesn't exist for some reason, the regular edge snap will be silently installed. Use `snap info multipass` to verify.
- On Windows and macOS:
  1. go to the summary page for the last successful GitHub Actions run for the PR
  2. scroll to the bottom, until you see "Artifacts"
  3. Download and install the appropriate package:
    * Windows: MSI package
    * macOS: pkg package

<img alt="image" src="images/pr-packages.png" />
