# Show HN 发布稿

> 面向英文社区（Hacker News）。语气：技术、诚实、不吹第一。发布前先 `make && ./jinvia run examples/thermostat-guard` 确认快速开始能跑。
> 链接：https://github.com/cjintao133-cell/jinvia

## 标题（三选一，推荐第 1）

1. `Show HN: Jinvia – a pure-C, zero-dependency sandbox for untrusted plugins and agent tools`
2. `Show HN: A tiny pure-C bytecode sandbox that runs where Wasmtime can't (~2k lines)`
3. `Show HN: Jinvia – pour your own sandbox floor: pure C, from an MCU to a server`

## 正文

Hi HN,

Jinvia (星垠) is a **pure-C11, zero-dependency capability sandbox**. Capabilities ship as portable bytecode and run in a least-privilege, default-deny VM. One C compiler is the entire prerequisite — no Rust, Go, JVM, or WASM runtime.

**Why.** AI agents and MCP servers are now executing a lot of untrusted third-party code. The usual answers are containers, microVMs, or WebAssembly (Wasmtime, Microsoft's Wassette). All good — but heavy. None of them fits a microcontroller, a no-OS device, or a plain C host. That bottom layer is what I wanted: a sandbox whose only floor is a C compiler.

**What's inside**
- A hand-written stack bytecode (JCB — *not* WASM): fixed-width, ~40 opcodes, i64 + f32/f64, linear memory. Tiny enough for one person to audit.
- Assembler + disassembler; a **load-time static verifier** (stack-height dataflow) that rejects bad bytecode before it ever runs.
- **DoS budgets** (steps / stack / frames) settable by the host.
- A **C-subset compiler** (`jinvia cc`), so authors write C, not assembly.

**Demo — "it tries to kill the host; the host doesn't blink."**
`examples/sandbox-showdown/` has 7 malicious capabilities: privilege escalation, host-index escape, divide-by-zero, `INT64_MIN/-1` (SIGFPE in a naive impl), out-of-bounds read, infinite loop, recursion bomb. Run `bash examples/sandbox-showdown/showdown.sh` — all 7 are blocked at load or runtime, zero crashes, zero escalation.

**Testing.** 16 unit suites, each run normal + ASan/UBSan + Valgrind; multi-million-sample custom fuzzing + libFuzzer; mutation tests. Fuzzing found and I fixed four real bugs (segment-dup leak, SIGFPE on `INT64_MIN/-1`, unbounded `nparams` stack overflow, `addr+8` unsigned-overflow OOB read).

**Honest status.** Not MCP-integrated yet (the MCP adapter is roadmap); no external security audit; cross-arch verified only at source level so far; the C subset has no arrays/pointers yet.

Curious whether "pure-C sandbox floor" is a real wedge, or just a fun toy. Roast away.

## 作者第一条评论（发布会后自己先贴，补充技术定位）

Author here. A few honest comparisons, since "another VM" deserves scrutiny:

- **vs wasm3 / WAMR**: those *implement WASM*. I deliberately don't — the WASM spec surface is huge to get right. A fixed-width ~40-op stack machine is something one person can reason about end-to-end and still pass ASan/Valgrind/fuzz cleanly. Trade-off: no WASM ecosystem. If you need WASM, use wasm3/WAMR.
- **vs Wassette (Microsoft)**: Wassette is Rust + Wasmtime + component-model, MCP-first. That's the right idea and far more productized. Jinvia is the *floor underneath* — pure C, no runtime, can land on an MCU. These are complementary, not competing.
- **vs q3vm / uvm32**: great prior art for tiny embeddable VMs. Jinvia adds three things: a load-time verifier, a C front-end, and a capability manifest (`needs:` / default-deny). q3vm gave me confidence this shape can ship for 20 years.

The security posture is three pillars: default-deny (undeclared host capabilities are simply not given), zero shared memory (capability talks to host only through declared calls), and same bytecode everywhere. Plus host-settable DoS budgets so a malicious loop/recursion dies in-budget instead of taking the host down.

Roadmap, in order: MCP adapter (turn an MCP tool into a capability package), C-subset arrays, cross-arch CI on QEMU/ARM, then an external audit. I'd rather ship an honest small thing than overclaim.

Happy to answer anything — especially "why not just WASM?" and "is a bespoke bytecode a mistake for ecosystem adoption?"
