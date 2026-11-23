# OpenSSF 评分与徽章（公开后点亮）

Jinvia 按 OpenSSF 的口径把「单人能拿的」都焊死；有些徽章只在**公开仓库**下才评分/生效，这里列清楚状态与操作。

## 现在就有效（README 已挂）
- **CI**（GitHub Actions，7 job：gcc/clang 门禁、coverage、portability、x86-32、aarch64 QEMU、cppcheck）
- **License: Apache-2.0**、**C11**、**0 第三方依赖**、**coverage 80.7%（核心运行时）**、**~270 MIPS**、**Release v0.1.0**、**MCP stdio server**、**PRs welcome**

## 公开后自动点亮（workflow 已备好在 `docs/publish/`）
| 徽章 | 来源 | 启用动作 |
|---|---|---|
| **OpenSSF Scorecard** | `ossf/scorecard-action` + `api.securityscorecards.dev` | 公开后把 `docs/publish/scorecard.yml` 放进 `.github/workflows/`，push 一次即评分并出徽章（约 8–9 / 10，单人天花板约 8.7–8.9） |
| **CodeQL** | GitHub CodeQL（公开仓库免费） | 公开后放 `docs/publish/codeql.yml` 进 `.github/workflows/` |
| **OpenSSF Best Practices (CII)** | bestpractices.dev 问卷 | 公开后到 https://www.bestpractices.dev 注册仓库 → 填问卷（多数条目 Jinja 已满足）→ 拿 badge markdown 粘进 README |

## CII Best Practices 自评（对 Jinvia 现状）
- ✅ 有 LICENSE（Apache-2.0）、有 SECURITY.md、有漏洞私密报告渠道（GitHub advisories）
- ✅ 自动化测试 + 覆盖率门禁 + fuzz + 多架构 CI
- ✅ 版本化 + 变更日志 + 语义化
- ✅ 文档（README 中英、SPEC、docs/mcp.md）
- ⏳ 需补：GOVERNANCE/ROADMAP（已有 ROADMAP 雏形可加）、release 签名（可选）、`AUTHORS`/`NOTICE`

## 单人 Scorecard 天花板（诚实）
- Code-Review=0、Contributors=0、Maintained/flags 需时间；单人规则上 Scorecard 9.0 基本到顶约 **8.7–8.9**。计划：day-1 把 Branch-Protection、Security-Policy、License、CI-Tests、Fuzzing、Pinned-Dependencies、Token-Permissions、SAST 等单人可满分项焊死。



## 最终状态（2026-11-23, OpenSSF Scorecard 实测 7.9/10）
- **OpenSSF Best Practices = Silver**（项目 17017），Scorecard 的 CII-Best-Practices 记 7/10。
- **Signed-Releases = 8/10**：发布用 cosign 无钥签名（`.sig` + `.pem`），v0.1.2 为签名发布。
- **Branch-Protection = 8/10**：main 用 repository ruleset（PR 审核 + code-owner 审核 + 必过 CI + 禁强推/删除 + 线性历史；admin 保留 bypass 以便直接推送）。
- 11 项满分：Security-Policy / CI-Tests / SAST(CodeQL) / Fuzzing / Vulnerabilities / Token-Permissions / Pinned-Dependencies / Dangerous-Workflow / License / Binary-Artifacts / Dependency-Update-Tool。
- 剩余低分是**结构性**、非代码能补：Code-Review 0（需他人审核 changeset，单人无法自审）、Maintained 0（建仓 <90 天）、Contributors 0（需 ≥2 个组织）、Packaging -1（纯 C 无 Scorecard 认可的发包渠道）。
- 结论：纯 C、单人、建仓数日的项目，7.9 已属高位（对比 star-river Go 内核 7.7）。
