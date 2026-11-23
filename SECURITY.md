# Security Policy

## Reporting a Vulnerability

Please **do not** open a public issue for security vulnerabilities.

Report privately through either channel:

- **GitHub private security advisory (preferred):** https://github.com/cjintao133-cell/jinvia/security/advisories/new
- **Email:** security@jinvia.dev

We aim to acknowledge within 7 days and to follow coordinated disclosure. Please include: affected version/commit, a description, and a minimal reproduction if possible.

Preferred-Languages: en, zh

## Scope

Jinvia executes untrusted bytecode; its purpose is containment. High-severity issues:

- Memory-safety bugs letting bytecode read/write outside its linear memory or the host.
- Sandbox escape: invoking host capabilities not declared/registered (default-deny must hold).
- DoS not stopped by the configured budgets (steps/stack/frames).
- Crashes (SIGFPE/SIGSEGV/abort) reachable from crafted bytecode.

## Current status (please read)

This project is **0.1.x (alpha)** and **not yet externally security-audited**. It is fuzzed (multi-strategy + libFuzzer), sanitizer-clean (ASan/UBSan/Valgrind) and scanned by CodeQL, but awaits independent review. **Do not claim "production-grade" for security-critical deployments yet.**

## Supported Versions

| Version | Supported |
| ------- | --------- |
| 0.1.x (alpha) | :white_check_mark: (best effort) |

## Disclosure

We follow coordinated disclosure. Once a fix is available, we will credit reporters (unless you prefer to stay anonymous).
