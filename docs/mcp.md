# Jinvia as an MCP server

Jinvia ships a **minimal Model Context Protocol (MCP) server over stdio**. Every capability package in a tools directory becomes an MCP *tool*; when an LLM calls a tool, its **untrusted bytecode runs inside the Jinvia sandbox** — default-deny, with DoS budgets and bounds checks. This is the point of the whole project: a **pure-C sandbox floor under AI tool execution.**

## Run it

```bash
make
./jinvia mcp <tools-dir>     # speak MCP JSON-RPC 2.0 over stdin/stdout
```

A *tools dir* is just a folder of capability packages:

```
my-tools/
  thermostat-guard/  capability.conf + main.jcb
  sum-1-to-10/       capability.conf + main.jcb
  ...
```

`examples/` already works as a tools dir (6 tools, including the malicious `evil` for a live default-deny demo).

## Wire it into an MCP client (e.g. Claude Desktop)

Add to the client's MCP config:

```json
{
  "mcpServers": {
    "jinvia": {
      "command": "/absolute/path/to/jinvia",
      "args": ["mcp", "/absolute/path/to/my-tools"]
    }
  }
}
```

The client will call `initialize`, `tools/list`, `tools/call` over stdio. Supported methods: `initialize`, `tools/list`, `tools/call`, `ping`, and notifications (ignored). Unsupported methods return JSON-RPC `-32601`.

## What a tool looks like on the wire

- `tools/list` → each tool = `{ name, description, inputSchema }` where `name`/`description` come from `capability.conf`.
- `tools/call` → the capability's entry runs with host capabilities **only if declared in `needs` and registered by the host**. Undeclared → refused: `{"content":[{"type":"text","text":"error: refused: undeclared host capability (default deny)"}],"isError":true}`.

Example (`sum-1-to-10`):

```
→ {"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"sum-1-to-10"}}
← {"jsonrpc":"2.0","id":3,"result":{"content":[{"type":"text","text":"result = 55"}],"isError":false}}
```

## Tool arguments (typed `inputSchema`)

Tools can take parameters. Declare them in `capability.conf`:

```
name: add
does: add two integers a + b
params: a, b           # a and b are integers (default type: int)
entry: main.jcb
```

`params` is comma-separated; each item is `name` (integer) or `name:type` where `type` is `int`, `bool`, or `string`. `tools/list` emits a real `inputSchema`:

```json
"inputSchema":{"type":"object","properties":{"a":{"type":"integer"},"b":{"type":"integer"}},"required":["a","b"],"additionalProperties":false}
```

`tools/call` maps the JSON `arguments` object to the VM by parameter name, in declaration order, and passes them straight into `j_vm_call`. All declared parameters are **required**; a missing / wrong-typed / out-of-range argument returns `isError:true` with a precise message (e.g. `missing required argument 'b'`, `argument 'a' must be an integer`). Values are still boxed as i64 inside the sandbox — a `bool` accepts `true/false` (or `0/1`).

How arguments map onto the VM (all values are i64 inside the sandbox):

- `int` → one i64 (must be an integer value; bools coerced to 0/1).
- `bool` → one i64 (accepts `true`/`false` or `0`/`1`).
- `string` → **two** i64 slots: `ptr` then `len`. The UTF-8 bytes are copied into the tool's **linear memory** (plus a trailing NUL), and the tool reads them with `MEM_LOAD8 ptr+i`. A `string` parameter therefore requires the capability to declare `.memory N`; otherwise the call fails with a clear error.

So the entry function's parameter count is the sum of slot widths (a `string` counts as 2). e.g. `params: s:string` → entry takes `(ptr, len)`. All declared parameters are **required**; a missing / wrong-typed / out-of-range argument returns `isError:true` with a precise message.

Example (`examples/add`, a+b):

```
→ {"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"add","arguments":{"a":2,"b":3}}}
← {"jsonrpc":"2.0","id":5,"result":{"content":[{"type":"text","text":"result = 5"}],"isError":false}}
```

Example (`examples/strlen`, string argument → byte length):

```
→ {"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"strlen","arguments":{"s":"hello"}}}
← {"jsonrpc":"2.0","id":6,"result":{"content":[{"type":"text","text":"result = 5"}],"isError":false}}
```

## Why this is different

Conventional MCP servers are **native binaries on the host** — an untrusted MCP tool runs with the host's full privileges. Jinvia runs the tool as *bytecode in a sandbox*: it can't touch the host except through the exact host callbacks the host registered, and it can't hang the host (step/stack/frame budgets). That is the security story Microsoft's Wassette tells with Wasmtime + Rust; Jinvia tells it in **pure C, no runtime, small enough for constrained targets.**

## Security posture (repeat)

- **Default-deny**: a tool sees only the host capabilities it declared *and* the host chose to register.
- **Budgets**: infinite loops / recursion bombs die in-budget, not by taking the host down.
- **Verified**: bytecode is statically verified at load; the JSON layer is fuzzed/ASan-clean.

## Status / roadmap (honest)

- ✅ stdio transport, `tools/list` + `tools/call`, default-deny.
- ✅ Tool arguments: `params` in `capability.conf` → typed `inputSchema`; `arguments` mapped by name into `j_vm_call` (**int / bool / string**; string via linear-memory `ptr`+`len`; all required).
- ⏳ Roadmap: HTTP/SSE transport, MCP **resources** & **prompts**, auth, per-tool host-capability policies, optional/default parameters, structured/string results (result is currently a single i64).
- ⚠️ **Not externally audited.** The JSON parser is minimal (no depth limit yet — treat long adversarial input with care).
