# Security Policy

## Scope

Jinvia runs **untrusted bytecode**. Its whole reason to exist is to contain malicious capabilities. Bugs that break that containment are high severity:

- Memory-safety bugs that let bytecode read/write outside its linear memory or the host.
- Sandbox escapes: invoking host capabilities that were not declared/registered.
- DoS that is **not** stopped by the configured budgets (steps/stack/frames).
- Crashes (SIGFPE/SIGSEGV/abort) reachable from crafted bytecode.

## Reporting

Please **do not** open a public issue for a security bug. Use GitHub's private vulnerability reporting (Security tab → "Report a vulnerability") or email the maintainer.

Include: a minimal `.jcbasm` / bytes reproducer, the exact command, and observed vs expected behavior.

## Status

- Fuzzed (custom multi-strategy + libFuzzer) and sanitizer-clean (ASan/UBSan/Valgrind).
- **Not yet externally audited.** Treat as a work draft for security-critical deployments.
