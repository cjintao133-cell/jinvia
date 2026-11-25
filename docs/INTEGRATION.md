# 10-Minute Integration (embed Jinvia in your C host)

Goal: run untrusted, host-registered capabilities safely from your own C program.

1. **Build** (a C compiler is the only prerequisite):
   ```sh
   make            # produces ./jinvia
   make lib        # (optional) static lib for embedding
   ```

2. **See the embedding pattern** — `examples/embed/embed_demo.c` shows the full host API:
   register capabilities, load a capability package, run it under budgets.

3. **Define what a tool may touch** — capabilities are declared in `capability.conf`:
   ```ini
   # only these named capabilities exist; everything else is denied by default
   params = x:int, name:string
   ```

4. **Run a package**:
   ```sh
   ./jinvia run examples/add        # run a capability bundle from a directory
   ./jinvia cc examples/add/main.c examples/add/main.jcbasm   # compile a C subset to bytecode
   ```

5. **As an MCP tool server** (drop into any MCP client):
   ```sh
   ./jinvia mcp ./tools             # each capability dir = one MCP tool, executed in-sandbox
   ```
   See `docs/mcp.md` for `tools/list` / `tools/call` schema (typed params via `params=`).

6. **Harden the host process** (defense in depth): run as non-root, read-only fs, drop capabilities,
   set pids/memory limits (the Dockerfile shows the exact flags).

Every failure mode (unauthorized capability, OOB, div-by-zero, runaway loop/recursion) is converted
to a **clean denial**, not a crash — see `examples/sandbox-showdown`.
