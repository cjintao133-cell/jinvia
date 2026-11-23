# Jinvia MCP server — container image for directory listings (Glama / awesome-mcp-servers).
# Pure C11, zero dependencies. Builds the engine, then runs `jinvia mcp` over stdio.
FROM gcc:13-bookworm AS build
WORKDIR /src
COPY . .
RUN make -j"$(nproc)"

FROM debian:bookworm-slim
LABEL org.opencontainers.image.title="jinvia" \
      org.opencontainers.image.description="Pure-C11 zero-dependency sandbox that runs a compiled C subset in-process; ships a minimal MCP stdio server." \
      org.opencontainers.image.source="https://github.com/cjintao133-cell/jinvia" \
      org.opencontainers.image.licenses="Apache-2.0"
WORKDIR /app
COPY --from=build /src/jinvia /app/jinvia
COPY --from=build /src/tools /app/tools
# MCP JSON-RPC 2.0 over stdin/stdout
ENTRYPOINT ["/app/jinvia", "mcp", "/app/tools"]
