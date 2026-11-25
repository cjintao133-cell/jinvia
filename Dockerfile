# syntax=docker/dockerfile:1
# Jinvia — hardened, self-hostable image.
# Pure C11, zero third-party dependencies. Runs the MCP stdio server as NON-ROOT,
# and is compatible with a read-only root filesystem.

# ---------- build ----------
FROM gcc:13-bookworm AS build
WORKDIR /src
COPY . .
RUN make -j"$(nproc)" && strip jinvia

# ---------- runtime ----------
FROM debian:bookworm-slim AS runtime
LABEL org.opencontainers.image.title="jinvia" \
      org.opencontainers.image.description="Pure-C11 zero-dependency sandbox that runs a compiled C subset in-process; ships a minimal MCP stdio server." \
      org.opencontainers.image.source="https://github.com/cjintao133-cell/jinvia" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.vendor="Jinvia"

WORKDIR /app
# 65532 = "nonroot" convention (distroless-compatible); no passwd entry needed.
COPY --from=build --chown=65532:65532 /src/jinvia /app/jinvia
COPY --from=build --chown=65532:65532 /src/tools  /app/tools

# Drop all privileges: numeric non-root user.
USER 65532:65532

# MCP JSON-RPC 2.0 over stdin/stdout. No ports, no network.
# Hardened run:
#   docker run --rm -i \
#     --read-only --tmpfs /tmp:rw,noexec,nosuid,size=8m \
#     --cap-drop ALL --security-opt no-new-privileges \
#     --pids-limit 128 --memory 128m --cpus 0.5 \
#     jinvia
ENTRYPOINT ["/app/jinvia", "mcp", "/app/tools"]
