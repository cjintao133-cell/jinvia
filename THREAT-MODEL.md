# Jinvia Threat Model

> Scope: what Jinvia's sandbox does and does **not** protect against.
> Honesty about limits is deliberate — trust comes from stating what it can't do.

## What Jinvia is
Jinvia executes **untrusted JCB bytecode** (produced from a restricted C subset) inside a
**default-deny** capability sandbox. A capability is a named, host-registered entry point;
bytecode can only ever call capabilities the host explicitly declared and granted.

## Assets / trust boundary
- **Untrusted input**: JCB bytecode + the tool arguments passed via MCP.
- **Trusted**: the Jinvia VM/verifier/host runtime, and the host application that registers capabilities.
- Boundary: everything on the bytecode side is attacker-controlled; the VM, verifier, linear memory
  and host-capability registry are trusted.

## In scope — Jinvia MUST stop these
1. **Memory safety escape**: bytecode reading/writing outside its own linear memory or host memory.
2. **Sandbox escape**: invoking a host capability that was not declared/registered (default-deny must hold),
   or otherwise reaching host syscalls/files/network that were not explicitly granted.
3. **Resource exhaustion** beyond configured budgets: instruction/step budget, stack depth, frame count.
4. **Host crashes** reachable from bytecode: SIGFPE / SIGSEGV / abort not converted to a clean denial.

## Out of scope — Jinvia does NOT protect against (by design / host's job)
- **Bugs in capabilities the host grants.** If you register a capability that opens files or sockets,
  that is your attack surface. Jinvia isolates the *bytecode*, not your host functions.
- **Network / filesystem / syscall isolation** at the OS level. Run the process with the OS's own
  controls (see Docker hardening: `--read-only`, `--cap-drop ALL`, `no-new-privileges`, seccomp,
  cgroups/pids/memory limits). Jinvia is one layer of a defense-in-depth stack, not the whole stack.
- **Side channels / timing.** Not mitigated.
- **Correctness of the attacker's algorithm.** Jinvia contains execution; it does not judge intent.
- **The restricted C subset compiler (`jinvia cc`).** Compiler correctness is a separate concern;
  the sandbox's guarantee is about the *bytecode it executes*, regardless of where it came from.

## Residual risk & recommended posture
- Run Jinvia as **non-root**, with a **read-only** root filesystem, **all capabilities dropped**,
  **pids/memory/cpu limits**, and `no-new-privileges`. The shipped Dockerfile + run recipe do this.
- Treat granted capabilities as a **policy surface**: only register the minimum set a tool needs.
- Fuzzing (1M+ samples under ASan/UBSan) and Valgrind clean runs are continuously enforced in CI;
  an external review of the sandbox boundary is on the roadmap.

## Reporting
See SECURITY.md — private advisories preferred, `security@jinvia.dev`.
