# Changelog

All notable changes to this project are documented here. Format based on [Keep a Changelog](https://keepachangelog.com/), versioning [SemVer](https://semver.org/).

## [0.1.4] - 2026-11-25

### Security
- **Sandbox: enforce iron law #1 (default-deny).** A capability may now only `HOST`-call a host whose name it declared in `needs`; previously any *registered* host was callable by index, so a capability declaring `read:temp` could still invoke `open:door`. Added a `name_in_needs` check in `jinvia_load` and an `examples/sneaky` regression case (denied with exit 3). Found via self-audit — see `docs/REVIEW.md`.
- **VM: wall-clock time budget.** In addition to the step limit, each call is now bounded by a wall-clock budget (default 2 s in CLI/MCP, checked every 4096 steps), so a slow or hostile capability can no longer hang the single-threaded MCP server. New `jinvia_set_time_budget()`. ABI-safe: `JLimits` is unchanged (budget kept internal).

### Docs
- README / README.en: TL;DR + 30-second quickstart on the first screen; added an honest **Limitations** section (MCU/bare-metal is a design goal, not yet hardware-validated; single-instance serial; ~270 MIPS is a micro-benchmark; not WASM-compatible; restricted C subset only).
- SPEC.md: corrected `main.wasm` -> `main.jcb` (JCB bytecode).
- Added `docs/REVIEW.md`: a security / robustness self-audit (what was found, verified, fixed — including two false alarms that were checked and dismissed).

### Verified
- C-GATE GREEN: 17 suites × (normal + ASan+UBSan + Valgrind) + CLI integration (incl. `undeclared-host bypass denied(3)`) + sandbox-showdown 7/7 + fuzz 250000×4.

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
- **MCP tool arguments**: `capability.conf` now accepts `params: a, b` (or `name:bool` / `name:string`); `tools/list` emits a typed `inputSchema` (integer/boolean/string, all required), and `tools/call` maps the JSON `arguments` by name into `j_vm_call`. `string` params are copied into the tool's linear memory and passed as `(ptr,len)` (requires `.memory N`). New `jinvia_call_args` / `jinvia_entry_nparams` / `jinvia_mem_size` / `jinvia_mem_write` API. Example tools `examples/add` (a+b), `examples/scale` (bool), `examples/strlen` (string).
- Fixed Windows/MinGW `-Werror` build: replaced non-portable `%zu` with `%llu` + cast in `jcb_cc.c` / `main.c` (MinGW CRT lacks `%zu`).
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
