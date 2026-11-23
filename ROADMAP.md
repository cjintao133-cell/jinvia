# Roadmap

Jinvia is a pure-C sandbox engine (JCB bytecode VM + C-subset compiler + MCP stdio server).

## Near term
- MCP: HTTP/SSE transport, MCP resources & prompts, per-tool host-capability policies, tool arguments.
- C subset: arrays, pointers, floating point.
- Packaging: pkg-config + `make install`, distro/package-manager presence.

## Mid term
- OSS-Fuzz integration; deeper fuzzing of the JSON/MCP layer.
- External security review of the sandbox boundary.

## Long term
- Deterministic replay tooling; more language front-ends that emit JCB.

Status is tracked in CHANGELOG.md; versions follow SemVer.
