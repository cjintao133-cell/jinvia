# Contributing to Jinvia

Thanks for your interest. Jinvia is a small, single-maintainer project — please be patient with reviews.

## Ground rules

- **C11, zero third-party dependencies.** No new external deps. Standard library only.
- **Every change must keep the gates green:** `bash test-c.sh` (normal + ASan/UBSan + Valgrind + fuzz), `bash cov.sh 80`, `bash portability.sh`.
- **Security is the point.** New code that parses bytes, walks the stack, or touches memory must come with tests, and ideally a fuzz input.
- **Don't over-claim.** Docs and comments must be honest (this is a work draft, not audited production).

## Workflow

```bash
make                 # build
bash test-c.sh       # full gate
make bench           # perf numbers
```

For anything non-trivial, open an issue first describing the change and why.

## Good first issues

- C-subset compiler: add `for`, `break/continue`, arrays.
- MCP stdio adapter (turn an MCP tool call into a capability package).
- More fuzz targets / OSS-Fuzz integration.

## Commit messages

Short imperative subject; explain the *why* in the body. Reference issues as `#123`.
