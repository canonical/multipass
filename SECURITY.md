# Security policy


## What is a vulnerability?

All vulnerabilities are bugs, but not every bug is a vulnerability. Vulnerabilities compromise one or more of:

- Confidentiality (personal and corporate confidential data)
- Integrity (trustworthiness and correctness)
- Availability (uptime and service)

If you discover a security vulnerability within Multipass, we encourage responsible disclosure.
If you're not sure whether you found a vulnerability, a bug, or something else, please use the process below for reporting a vulnerability.
We will then assess and triage accordingly.

## Reporting vulnerabilities

Multipass accepts private reports of security vulnerabilities submitted through
[GitHub security advisories](https://docs.github.com/en/code-security/security-advisories/working-with-repository-security-advisories/about-repository-security-advisories).
For detailed instructions, please review GitHub documentation on [privately reporting a security vulnerability](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability).

### How to submit a report

1. Go to the [Security Advisories](https://github.com/canonical/multipass/security/advisories) page on the `multipass` repository.
2. Click "Report a vulnerability".
3. Provide detailed information about the vulnerability, including steps to reproduce, affected versions, and potential impact.
4. Click "Submit report".

## Our response process

Vulnerabilities are classified by risk, which factors in impact and likelihood.
The Multipass project will prioritize responding to all High and Critical severity vulnerabilities.

When we receive an issue, we will work with the reporter to determine how best to proceed.
After a fix is available for a confirmed vulnerability, we will also coordinate disclosing and releasing it to the various platforms.

## Supported versions

Multipass is released as a snap on Linux, an MSI package on Windows, and an installer package on macOS.
We support the latest stable version of Multipass on each of these platforms.
Multipass notifies you of newer releases on all platforms.
On Linux, updates are handled automatically by the snap machinery.

Please ensure you are using the latest version to benefit from the latest patches.

### Release of security fixes

Security updates are distributed with a new release, which becomes the new supported version.
This can be either a bug-fix, minor, or major release, depending on what other modifications it includes.

The urgency of the fixes included - security and otherwise - determines the urgency of the release.
We are committed to fixing high-risk security issues and releasing them as quickly as possible.

Additionally, the `candidate` channel of the Multipass snap provides frequent rebuilds of the `stable` channel with updated dependencies.
These builds are produced from the same Multipass code, but with up-to-date `.deb` packages.
Candidate snaps are promoted to the stable channel from time to time.

### How to check your version

You can find out which version of Multipass is installed on your system with the command:

```
multipass version
```

The `snap` command provides information on exact revisions and build dates:

```
snap info multipass
```
