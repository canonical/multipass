# Install CI builds

To install/test CI builds, the [gh-install GitHub CLI extension](https://github.com/sharder996/gh-install) is very useful.

It downloads and installs the Multipass package built by CI for a given pull request.

Think of it as `gh co 5135`, but instead of checking out the code it installs the package produced by that PR's CI run, for your current OS. With no arguments it installs the latest nightly build.

- [https://github.com/sharder996/gh-install](https://github.com/sharder996/gh-install)
- Install with `gh extension install sharder996/gh-install`
