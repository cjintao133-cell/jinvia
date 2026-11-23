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

## 当前状态（转 public 后，2026-11-23）
- **仓库已 public**：https://github.com/cjintao133-cell/jinvia
- **CI**：✅ 绿（run on 9296e97）
- **CodeQL**：✅ 绿（c-cpp 分析通过，SARIF 已上传到 Security 页）
- **OpenSSF Scorecard**：工作流已就位并触发，但 action 运行**失败**——原因是 GitHub 托管 runner 拉取 `gcr.io/openssf/scorecard-action` 镜像时报 Google GCR「project requires billing」（**外部基础设施故障，非本仓库配置**）。已加 `workflow_dispatch` 手动重跑 + 每周一 cron 自动重试；Google 侧恢复后，`api.securityscorecards.dev` 会发布分数、README 的 Scorecard 徽章自动变成具体分值。届时可再 `git` 触发一次或点 Actions 里「Re-run」。
- **CII Best Practices**：需在 bestpractices.dev 注册并填问卷后拿 badge（见 PUBLISH.md 第 3 步）。

## Scorecard 现状快照与"为何比 Go 内核低"（2026-11-23, main=bbed1cb, 6.5/10）

12 项满分(10/10)：Security-Policy, CI-Tests, SAST(CodeQL), Fuzzing, Vulnerabilities, Token-Permissions,
Pinned-Dependencies, Dangerous-Workflow, License, Binary-Artifacts, Dependency-Update-Tool。
与 star-river(Go, 7.7/10) 逐项对比，jinvia 在**所有安全/质量项上不相上下或更好**。差距全部来自 3 项：

| 项 | star-river | jinvia | 为什么 |
|---|---|---|---|
| CII-Best-Practices | 7 | 0 | **就是这个(Silver)**：需在 bestpractices.dev 注册本仓库并填问卷；star-river 注册过，jinvia 没有。**只能仓库所有者本人登录操作**（OAuth），约 10 分钟。 |
| Packaging | 10 | -1 | Go 项目自带 `go.mod` 视作可分发包；C 无 Scorecard 认可的发包渠道。试过 docker 打包→v2.4.3 内置旧版 Scorecard 不识别，反而把 Pinned-Dependencies 拖到 7，已回滚。 |
| Signed-Releases | 4 | 0 | star-river 的 release 有签名(部分)；jinvia 的 release 未做 GPG/sigstore 签名。 |

**结论**：加上 CII 徽章(+7) 后，jinvia 约为 7.5–7.8，即与 star-river 持平或更高。这一步需要你本人注册（你给 star-river 做过，同样流程）。
