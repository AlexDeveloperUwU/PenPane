# Security policy

## Supported versions

Only the latest release receives security fixes. PenPane has not reached 1.0, so fixes ship in the next release.

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub security advisories: open the Security tab of the repository and choose "Report a vulnerability". Do not open a public issue or pull request for a security problem.

Include the version (the file properties of `penpane.exe` show it), your Windows version, the steps to reproduce, and what an attacker gains. A short proof of concept helps.

You can expect an acknowledgement within a week and a status update within two weeks after that. Reporters are credited in the release notes unless they ask not to be.

## Scope

In scope:

- Memory-safety problems in PenPane's own code, for example in the configuration parser, the PNG encoder or the clipboard export.
- Anything that lets a file next to the executable (`penpane.ini`, `ink.ini`) or a crafted pointer or keyboard input run code, write outside the configuration file, or crash the program.
- A way to make the tool panels appear in a screen capture when the capture exclusion is switched on and the Windows version supports it.
- Weaknesses in the build and release scripts, such as an unchecked download.

Out of scope:

- Problems that need administrator rights or write access to the folder of the executable to begin with.
- Windows limits on drawing over elevated windows, exclusive full-screen applications or the secure desktop. They are listed as limitations in the README.
- Bugs without a security impact: use the bug report template.

PenPane makes no network connections, collects no data and writes only `penpane.ini` next to the executable and the Windows Run registry value that Start with Windows controls.
