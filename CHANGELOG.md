# Changelog

All notable changes to this project are documented here. Format based on [Keep a Changelog](https://keepachangelog.com/), versioning [SemVer](https://semver.org/).

## [0.1.3] - 2026-11-23

### Fixed
- Memory safety (from static analysis): no longer double-free a realloc'd pointer in `jcb_asm` (split realloc checks); don't free `ins` before handing it to the caller in `jcb_verify`; guard nullable locals in `jcb_vm`.
- `mcp`: `jesc()` guards a NULL string (prevents a crash when an incoming JSON field is missing).
- Portability: replace POSIX-only `getline()` with a portable `read_line()` — fixes the Windows/mingw build.
- Explicit integer casts in `jcb_read`, `jcb_vm`, `jcb_asm` for clean `-Wconversion -Wsign-conversion` builds.

### Verified
- gcc/clang {c11,gnu11,c17} x {O0,O2,O3,Os}; ASan+UBSan, Valgrind; clang-analyzer and cppcheck clean.
- Builds & runs on Linux (x86-64, aarch64), Alpine/musl, and Windows (mingw). `-Wconversion -Werror` clean on gcc and clang. C-GATE GREEN.

## [Unreleased]

### Added
- Expanded MCP tests (JSON escapes, surrogate pairs, nested args, error paths) and C-compiler negative tests; MCP JSON layer coverage 57%->76%.
- README badge set (CI, license, C11, 0-deps, coverage, throughput, release, platforms, MCP, PRs); prepared OpenSSF Scorecard + CodeQL workflows under `docs/publish/`.
- **MCP stdio server** (`jinvia mcp <tools-dir>`): exposes capability packages as MCP tools; `tools/call` runs untrusted bytecode inside the sandbox with default-deny. Methods: initialize / tools/list / tools/call / ping. Self-contained JSON parser (zero deps). Docs: `docs/mcp.md`.
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
