# Changelog

All notable changes to this project are documented here. Format based on [Keep a Changelog](https://keepachangelog.com/), versioning [SemVer](https://semver.org/).

## [Unreleased]

### Added
- Pure-C **C-subset compiler** (`jinvia cc in.c out.jcbasm`): `int` globals/locals, functions/recursion, arithmetic, comparisons, `&& || !`, `if/else/while/return`, `extern int host(int)`. Tests: normal + ASan/UBSan + Valgrind.
- `examples/cc/` — five runnable C programs (fib, gcd, primes, collatz, sensor-double).
- `examples/sandbox-showdown/` — 7 malicious capabilities, all blocked; `showdown.sh` (+ `--check`).
- `make bench` — throughput / load / latency numbers.
- GitHub Actions CI: gcc/clang gate, coverage, portability, x86-32, aarch64 via QEMU, cppcheck.
- Apache-2.0 `LICENSE` file; English `README.en.md`.

### Fixed
- `Makefile` omitted `src/jinvia.c` (link failure).
- C-subset compiler: `memcpy(dst, NULL, 0)` UB; freeing uninitialized state on early-error paths.

## [0.1.0] — unreleased
- Initial engine: JCB bytecode, VM with DoS budgets, default-deny capability gate, assembler/disassembler, load-time verifier, library API.
