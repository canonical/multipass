# Security Policy


## What is a vulnerability?

All vulnerabilities are bugs, but not every bug is a vulnerability. Vulnerabilities compromise one or more of:

- Confidentiality (personal or corporate confidential data).
- Integrity (trustworthiness and correctness).
- Availability (uptime and service).

If you discover a security vulnerability within Multipass, we encourage responsible disclosure. 
If you're not sure whether you found a vulnerability, a bug, or neither, please use the process below for reporting a vulnerability. 
We will then assess and triage accordingly.

## Reporting a Vulnerability

Multipass accepts private reports of security vulnerabilities made through 
[GitHub Security Advisories](https://docs.github.com/en/code-security/security-advisories/working-with-repository-security-advisories/about-repository-security-advisories). 
Here is the link to open a [new security adivory](https://github.com/canonical/multipass/security/advisories/new). 
For detailed instructions, please review the documentation on [privately reporting a security vulnerability](https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability).

### Steps to Report a Vulnerability

1. Go to the [Security Advisories Page](https://github.com/canonical/multipass/security/advisories) of the `multipass` repository.
2. Click "Report a Vulnerability."
3. Provide detailed information about the vulnerability, including steps to reproduce, affected versions, and potential impact.

## Response to vulnerabilities

Vulnerabilities are classified by risk, which factors in impact and likelihood. 
The Multipass project will prioritize responding to all High and Critical severity vulnerabilities.

When we receive an issue, we will work with the reporter to determine how best to proceed. 
After a fix is available to a confirmed vulnerability, we will also coordinate disclosure with releasing to the various platforms.

## Supported version

Multipass is released as a snap on Linux, an MSI package on Windows, and an installer package on macOS. 
In each of these platforms, we support the latest stable version of Multipass.
Multipass notifies you of newer releases on Windows and macOS.
On Linux, updates are handled automatically by the snap machinery.

Please ensure you are using the latest version, to benefit from the latest patches.

### Release of security fixes

Security updates are distributed with a new release, which becomes the new supported version.
This can be either a bug-fix, minor, or major release, depending on what other modifications it includes.

The urgency of the fixes included - security and otherwise - determines the urgency of the release.
We are committed to fixing high-risk security issues and releasing them as quickly as possible.

In addition, the candidate channel of the Multipass snap provides frequent rebuilds of the stable channel with updated dependencies.
These builds are produced from the same Multipass code, but with up-to-date deb packages.
Candidate snaps are later promoted to the stable channel from time to time.

### Finding versions

Multipass can tell you its version with:

```
multipass version
```

The snap command provides information on exact revisions and build dates:

```
snap info multipass
```
