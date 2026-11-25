# Jinvia — Buyer Trust Notes (Self-Hosted Edition)

If you are evaluating Jinvia for purchase as a self-hosted product, this page answers the
questions procurement/security teams actually ask.

## 1. License & what you are paying for
- Jinvia open-source core is **Apache-2.0**: you may use, modify, and **commercialize** it for free,
  including in closed-source products (keep LICENSE + NOTICE).
- The **paid self-hosted edition** sells *convenience & assurance*, not a different license:
  a hardened non-root image, a one-command deploy, an SBOM, threat model, integration support,
  and a support window. You are paying for *time saved and risk reduced*, not for permission to use.

## 2. Supply chain
- **Zero third-party dependencies** (pure C11, libc only) → the SBOM is intentionally tiny; there is
  no transitive npm/pip/cargo tree to exploit.
- A **CycloneDX SBOM** is produced on every release (`.github/workflows/sbom.yml`).
- Docker base images are pinned in CI; recommended to reference the image by **digest**.

## 3. Image security posture (the shipped Dockerfile)
- **Non-root** (`USER 65532`), no shell in the runtime layer, `drop privileges`.
- Compatible with **`--read-only`** root fs (only needs a writable `/tmp` tmpfs if you enable tracing).
- Runs with **no network** and **no open ports** (MCP over stdio).
- Recommended run flags: `--cap-drop ALL --security-opt no-new-privileges --pids-limit 128 --memory 128m`.

## 4. Test evidence (measured, reproducible)
Run `make test` (needs gcc/make/valgrind). Current state on this build:
- Unit/integration suites: **PASS** under plain build, **ASan+UBSan**, and **Valgrind** (0 fail).
- **Sandbox showdown: 7/7** malicious capabilities blocked (unauthorized call, host escape,
  div-by-zero, SIGFPE, out-of-bounds read, infinite loop, unbounded recursion).
- **Fuzzing: 1,000,000 samples** (random bytes / bitflip / indels / arithmetic) under ASan+UBSan —
  **no crash, no leak, no UB**.
- **Portability gate GREEN**: signed & unsigned char builds, strict alignment warnings, aarch64 cross-compile.
- **Performance** (4C4G, gcc -O2, single thread): ~266 MIPS, load ~1.0 µs, fib(24) ~9.4 ms. `make bench`.

## 5. Honest limits
See `THREAT-MODEL.md`. Jinvia isolates untrusted **bytecode**; it does not replace OS-level isolation
(seccomp/cgroups/read-only). We say so on purpose — a sandbox vendor that claims it protects
everything is not trustworthy.

## 6. Support & updates
- Security reporting: `security@jinvia.dev` (see SECURITY.md).
- Paid edition includes a support window & priority fixes (terms in the product listing).
