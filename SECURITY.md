# Security Policy

## Supported Versions

| Version | Supported |
| ------- | --------- |
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

Please **do not** open a public issue for a security bug. Report privately:

- GitHub: repo **Security** tab -> "Report a vulnerability" (private advisory), or
- Email the maintainer (see the GitHub profile).

Please include a minimal reproducer (`.jcbasm`/`.c`/raw bytes), the exact command, and observed vs expected behavior.

## What counts as a security bug (scope)

Jinvia runs untrusted bytecode; its whole purpose is containment. High-severity:

- Memory-safety bugs letting bytecode read/write outside its linear memory or the host.
- Sandbox escape: invoking host capabilities not declared/registered (default-deny must hold).
- DoS not stopped by the configured budgets (steps/stack/frames).
- Crashes (SIGFPE/SIGSEGV/abort) reachable from crafted bytecode.

## Disclosure Policy

We follow coordinated disclosure: acknowledge within a few days, aim to fix and credit reporters in release notes.

## Status

Fuzzed (multi-strategy + libFuzzer) and sanitizer-clean (ASan/UBSan/Valgrind); scanned by CodeQL. **Not yet externally audited** — treat as a work draft for security-critical deployments.
