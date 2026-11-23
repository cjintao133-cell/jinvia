# Building & embedding Jinvia

Jinvia is pure C11 with **zero third-party dependencies**. A C compiler is the whole toolchain.

## Build the CLI

```bash
make            # produces ./jinvia
make test       # full gate: normal + ASan/UBSan + Valgrind + fuzz + CLI + sandbox showdown
make bench      # throughput / latency numbers
```

## Build the library

```bash
make lib        # build/libjinvia.a and build/libjinvia.so
```

Embed as in `examples/embed/embed_demo.c`:

```c
#include "jinvia/jinvia.h"
JinviaRuntime *rt = jinvia_new();
jinvia_register_host(rt, "read:temp", my_read_temp, NULL);
jinvia_load(rt, conf_text, jcb_bytes, jcb_len);
jinvia_call(rt, &result);
jinvia_free(rt);
```

Minimal embeddable core is ~10 KB of text (read + decode + VM); the full engine (with assembler/disassembler/API) is ~52 KB. The C-subset compiler (`src/jcb_cc.c`) is build-time only and need not be linked into a runtime that only executes bytecode.

## Cross-compiling

The engine is plain C11 with no platform assumptions; it builds for x86-32 and aarch64 (CI runs both, aarch64 under QEMU). For MCU targets, compile the minimal core (`jcb_read` + `jcb_decode` + `jcb_vm`) only.
