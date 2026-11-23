# Jinvia · 星垠

> **An infinite river of stars — it lands on anything.**
> **Write once, run anywhere a C compiler reaches.**

[中文](README.md) · English

[![CI](https://github.com/cjintao133-cell/jinvia/actions/workflows/ci.yml/badge.svg)](https://github.com/cjintao133-cell/jinvia/actions/workflows/ci.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
![C11](https://img.shields.io/badge/C11-pure%20C-blue.svg)
![Dependencies](https://img.shields.io/badge/third--party%20deps-0-brightgreen.svg)
![Coverage](https://img.shields.io/badge/coverage-80.7%25%20core-brightgreen.svg)
![Throughput](https://img.shields.io/badge/throughput-~270%20MIPS-blue.svg)
[![Release](https://img.shields.io/github/v/release/cjintao133-cell/jinvia?include_prereleases&sort=semver)](https://github.com/cjintao133-cell/jinvia/releases)
![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20MCU-informational.svg)
![MCP](https://img.shields.io/badge/MCP-stdio%20server-blueviolet.svg)
![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)
[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/cjintao133-cell/jinvia/badge)](https://scorecard.dev/viewer/?uri=github.com/cjintao133-cell/jinvia)

![Jinvia sandbox showdown — 7/7 malicious capabilities blocked · fib(20)=6765](docs/assets/demo.gif)

Jinvia is a **pure-C11, zero-dependency capability sandbox**. Capabilities ship as portable bytecode (**JCB**) and run in a least-privilege, default-deny VM. One C compiler is the entire prerequisite — no Rust, Go, JVM, or WASM runtime to install.

## Why now: a pure-C sandbox floor for AI tools

Since 2025, AI agents and MCP servers execute a lot of **untrusted third-party code**. Microsoft's answer is [Wassette](https://github.com/microsoft/wassette): run MCP tools as WebAssembly components inside Wasmtime. The direction is right — but it's **heavy**: Rust toolchain, a WASM component ecosystem, and Wasmtime itself won't fit an MCU, a no-OS device, or a plain C host.

Jinvia pours the **bottom layer of that same floor**:

- **Pure C11, zero third-party deps** — one C compiler is the whole toolchain.
- **Tiny sandbox** — default-deny + resource budgets + bounds checks; malicious bytecode is stopped on the spot (see *Sandbox showdown*).
- **Isomorphic** — the same capability package runs unchanged from a chip to a server.

> Jinvia **ships a minimal MCP stdio server** (`jinvia mcp <tools-dir>`): every capability package is an MCP tool, and `tools/call` runs it inside the sandbox, default-deny. See [`docs/mcp.md`](docs/mcp.md). Still on the roadmap: HTTP transport, resources/prompts, auth, argument passing.

## Performance & footprint

> 4-core/4 GB cloud VM, gcc -O2, single thread. This is an **interpreter** (no JIT) — optimized for "runs on any C compiler", not for raw speed. Reproduce with `make bench`.

| Metric | Measured |
|---|---|
| Execution throughput | **~270 MIPS** (~3.7 ns/instruction) |
| Recursive fib(24) | 46368 in ~9 ms |
| Load (parse + verify + build VM) | **~0.9 µs**, module bytecode 184 bytes |
| VM core `jcb_vm` | **~7 KB** text |
| Minimal embeddable core (read + decode + VM) | **~10 KB** text |
| Full engine `.so` (assembler/disasm/API) | **~52 KB** text |
| C-subset compiler `jcb_cc` | ~18 KB (**build-time only**, not linked at runtime) |

## Quick start

```bash
make                                   # one C compiler is all you need
./jinvia asm examples/thermostat-guard/main.jcbasm examples/thermostat-guard/main.jcb
./jinvia run --trace examples/thermostat-guard
./jinvia dis examples/thermostat-guard/main.jcb   # disassemble to audit
```

## One capability = two files

```
my-capability/
  capability.conf   # manifest: one line each, plain text, zero-dep parse
  main.jcb          # implementation: JCB bytecode (from .jcbasm / from C)
```

```
name: thermostat-guard
does: read temperature once; if > 30 trigger the alarm
needs: read:temp, call:alarm
entry: main.jcb
```

## Swap the package — same engine, different identity

| Capability | needs | Result |
|---|---|---|
| `thermostat-guard` | read:temp, call:alarm | 34°C > 30 → 🔔 alarm, returns 1 |
| `sum-1-to-10` | *(none)* | returns 55 |
| `door-guard` | read:motion, open:door | motion → 🔓 open door, returns 1 |
| `evil` (counter-example) | read:temp, **write:disk** | **refused** (unauthorized, exit 3) |

> One kernel, change the package, change the identity — **it is blank, so it can be anything.**

## Write capabilities in C (no hand-written assembly)

Assembly is for engine tests, not for authors. Authors get a **C-subset compiler**:

```bash
./jinvia cc  fib.c  fib.jcbasm      # C subset -> readable, auditable assembly text
./jinvia asm fib.jcbasm fib.jcb     # assemble -> bytecode
```

Supports: `int` globals/locals, functions (params/return/recursion), `+ - * / %`, comparisons, `&& || !`, `if/else`, `while`, `return`, and `extern int host(int);` mapped to host capabilities. See `examples/cc/` (`bash examples/cc/build.sh` builds all). Not yet: arrays, pointers, float.

## Embed as a library

```c
#include "jinvia/jinvia.h"

int64_t my_read_temp(void *ud, int64_t arg) { return 34; }

JinviaRuntime *rt = jinvia_new();
jinvia_register_host(rt, "read:temp", my_read_temp, NULL);   // idx 0
jinvia_set_limits(rt, 1000000, 4096, 256);                   // budget
if (jinvia_load(rt, conf_text, jcb_bytes, jcb_len) == JINVIA_OK)
    jinvia_call(rt, &result);
jinvia_free(rt);
```

A capability whose `needs` are not all registered is refused with `JINVIA_ERR_NEEDS` (default deny). See `examples/embed/embed_demo.c`.

## Static verifier & disassembler

- **Load-time static verification** (`j_verify`): stack-height dataflow analysis — rejects stack underflow, inconsistent branch heights, and dangling function ends **before execution**.
- **Disassembler** (`jinvia dis file.jcb`): bytecode back to readable text, for audit and debugging.

## Security model (three pillars)

1. **Default-deny** — capabilities get nothing they didn't declare; a HOST call not declared at load time is rejected.
2. **Zero shared memory** — a capability talks to the host only through declared host calls.
3. **Nowhere it can't land** — the same bytecode runs unchanged from MCU to server.

Hard boundaries (implemented + tested): DoS budgets (steps/stack/frames) settable by the host; i64 add/sub/mul wrap with no UB; div/mod guard `y==0` and `INT64_MIN/-1` (no SIGFPE); parse/verify-time rejection of over-limit fields, duplicate sections, unknown opcodes, out-of-range indices.

## Sandbox showdown: it tries to kill the host; the host doesn't blink

`examples/sandbox-showdown/` has **7 malicious capabilities**: privilege escalation, host-index escape, divide-by-zero, `INT64_MIN/-1` (SIGFPE in a naive impl), out-of-bounds read, infinite loop, recursion bomb.

```bash
bash examples/sandbox-showdown/showdown.sh          # narrated (colors)
bash examples/sandbox-showdown/showdown.sh --check   # CI: 0 exit only if 7/7 blocked
```

Result: **all 7 blocked** (2 at load by the capability gate, 5 at runtime by the arithmetic/memory/budget guards), zero crashes, zero escalation.

## Run as an MCP server (sandboxed AI tools)

Jinvia ships a **pure-C, zero-dependency minimal MCP stdio server**: point it at a directory of capability packages and each becomes an MCP tool; an LLM's `tools/call` runs that tool's bytecode *inside the sandbox*.

```bash
./jinvia mcp examples          # use examples/ as the tools dir, speak MCP JSON-RPC over stdio
```

Wire into an MCP client (e.g. Claude Desktop):

```json
{ "mcpServers": { "jinvia": { "command": "/abs/path/jinvia", "args": ["mcp", "/abs/path/my-tools"] } } }
```

- `tools/list` → each capability package = one tool (name/description from `capability.conf`)
- `tools/call` → runs in the sandbox; a host capability **not declared in `needs` and not registered → refused** (`isError:true`)
- Methods: `initialize` / `tools/list` / `tools/call` / `ping`; others → `-32601`

> The difference that matters: a conventional MCP server is a **native binary on the host** — an untrusted tool gets the host's full privileges. Jinvia runs the tool as **bytecode inside a sandbox**: it touches the host only through the callbacks the host explicitly registered, and it can't hang the host (step/stack/frame budgets). See [`docs/mcp.md`](docs/mcp.md).

## JCB bytecode (ours, not WASM)

Fixed-width little-endian, ~40-op i64 stack machine with linear memory, globals and f32/f64. See [`SPEC_JCB.md`](SPEC_JCB.md).

### Why not WASM?

The WASM spec surface is huge to implement correctly; one person can't easily make it *right + auditable + ASan/Valgrind/fuzz clean*. Jinvia uses a fixed-width ~40-op stack machine one person can reason about end to end. The trade-off is explicit: **no WASM compat, no WASM toolchain/packages.** If you want the WASM ecosystem, use wasm3 / WAMR — they're better at that. Jinvia targets "places even a WASM runtime won't fit, plus a built-in C front-end."

## Status (honest)

- ✅ **Tests**: 17 suites (each normal + ASan/UBSan + Valgrind, incl. 3-mode C-compiler and MCP suites), multi-million-sample custom fuzzing + libFuzzer, mutation tests. CI on gcc/clang, x86-32, and aarch64 via QEMU.
- ✅ **4 real bugs found by fuzzing and fixed**: duplicate-section leak, `INT64_MIN/-1` SIGFPE, unbounded `nparams` stack overflow, `addr+8` unsigned-overflow OOB read.
- ✅ **Source-level portability**: char signedness matrix, `-Wcast-align=strict` / `-Wshadow` clean.
- ✅ **Ships a minimal MCP stdio server** (tools/list + tools/call, default-deny).
- ✅ **Coverage**: core runtime (VM/loader/verifier/assembler — the layer that executes untrusted bytecode) **80.7%** (CI gate ≥80%); whole tree incl. C compiler + MCP JSON layer **75.5%**.
- ⚠️ **No external security audit**; C subset has no arrays/pointers/float yet; MCP is stdio-only (no HTTP/resources/prompts yet).

## Directory

```
include/jinvia/   cap.h  jcb.h  cc.h  jinvia.h
src/              cap / jcb_read / jcb_decode / jcb_vm / jcb_asm / jcb_verify / jcb_disasm / jcb_cc / jinvia / main
tests/            unit suites + fuzz_jcb + fuzz_libfuzzer + fuzz_asm + bench
examples/         thermostat-guard / ... / evil  + cc/ (C sources) + sandbox-showdown/ (7 attacks)
SPEC.md  SPEC_JCB.md  test-c.sh  portability.sh  cov.sh  lib.sh  fuzz-soak.sh
.github/workflows/ci.yml
```

## License

[Apache-2.0](LICENSE) © Jinvia contributors. Use it, modify it, embed it in closed-source products (keep the copyright and license notices).
