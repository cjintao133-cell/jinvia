#ifndef JINVIA_MCP_H
#define JINVIA_MCP_H
/* SPDX-License-Identifier: Apache-2.0 */
/* 最小 MCP（Model Context Protocol）stdio 服务器。
 * 把 tools_dir 下每个能力包(capability.conf + main.jcb)暴露为一个 MCP tool；
 * tools/call 时在 Jinvia 沙箱里执行该能力的字节码，默认拒绝未授权宿主能力。
 * 纯 C11 + POSIX，无第三方依赖（自带 JSON 解析）。
 * 协议：newline-delimited JSON-RPC 2.0 over stdin/stdout。
 * 返回 0 表示正常结束。 */
#include <stdio.h>
int j_mcp_serve(const char *tools_dir, FILE *in, FILE *out);
/* 便于测试：处理一行请求，最多写出一个响应到 out（无响应则不写）。
 * 返回 0 成功（无论是否产生响应）。 */
int j_mcp_handle_line(const char *tools_dir, const char *line, size_t len, FILE *out);
#endif
