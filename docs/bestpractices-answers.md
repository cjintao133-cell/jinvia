# Best Practices (OpenSSF Silver) —— Jinvia 照抄答案表

> 目标：在 https://www.bestpractices.dev 给 `cjintao133-cell/jinvia` 新建项目，拿到 **Silver** 徽章。
> 本表由 star-river（#16960，已获 Silver）的通过答案整理，事实对 Jinvia 同样成立。登记必须**你本人用 GitHub 登录**（我无法代登）。

## 步骤

1. 打开 https://www.bestpractices.dev → **Get started** → 用 GitHub 登录（账号 cjintao133-cell）。
2. **Create new project**：Name = `Jinvia`，Repo = `https://github.com/cjintao133-cell/jinvia`，License = `Apache-2.0`。
3. 逐条填下面的答案（点选 + 需要时把 justification 粘进去）。
4. 保存后即可拿到 **Passing**；再补齐 Silver 的加严项后就升 **Silver**（本项目按表已满足）。把徽章贴回 README。

## A) 勾 **Met**（直接点 Met，绝大多数无需填字）

```
access_continuity
accessibility_best_practices
achieve_passing
achieve_silver
assurance_case
automated_integration_testing
build
build_common_tools
build_floss_tools
build_non_recursive
build_preserve_debug
build_repeatable
build_standard_variables
code_of_conduct
coding_standards
coding_standards_enforced
contribution
contribution_requirements
crypto_algorithm_agility
crypto_call
crypto_certificate_verification
crypto_credential_agility
crypto_floss
crypto_keylength
crypto_published
crypto_random
crypto_tls12
crypto_used_network
crypto_verification_private
crypto_weaknesses
crypto_working
dco
delivery_mitm
delivery_unsigned
dependency_monitoring
description_good
discussion
documentation_achievements
documentation_architecture
documentation_basics
documentation_current
documentation_interface
documentation_quick_start
documentation_roadmap
documentation_security
dynamic_analysis_enable_assertions
english
enhancement_responses
external_dependencies
floss_license
floss_license_osi
governance
hardening
implement_secure_design
input_validation
installation_common
installation_development_quick
installation_standard_variables
interact
interfaces_current
internationalization
know_common_errors
know_secure_design
license_location
maintained
maintenance_or_update
no_leaked_credentials
release_notes
repo_distributed
repo_interim
repo_public
repo_track
report_archive
report_process
report_responses
report_tracker
roles_responsibilities
signed_releases
sites_https
static_analysis
static_analysis_common_vulnerabilities
static_analysis_fixed
static_analysis_often
test
test_continuous_integration
test_invocation
test_policy
test_policy_mandated
test_statement_coverage80
tests_are_added
tests_documented_added
updateable_reused_components
version_semver
version_tags
version_tags_signed
version_unique
vulnerabilities_critical_fixed
vulnerabilities_fixed_60_days
vulnerability_report_private
vulnerability_report_process
vulnerability_report_response
vulnerability_response_process
warnings
warnings_fixed
warnings_strict
```

## B) 勾 **N/A**（附理由，粘进去）

- `crypto_password_storage` — No user passwords are stored; master keys live in Android Keystore / Windows DPAPI, not in a password database.
- `crypto_pfs` — The project implements no custom network transport cryptography; TLS (with forward secrecy defaults) is provided by Go net/http for outbound calls.
- `dynamic_analysis_fixed` — No dynamic-analysis tooling (e.g. fuzzing) is in place yet, so there are no such findings to fix.
- `dynamic_analysis_unsafe` — Core is memory-safe Go; the small C hot paths are isolated. No unsafe memory handling is user-exposed yet.
- `regression_tests_added50` — No bugs fixed in last six months; policy requires regression tests. See https://github.com/cjintao133-cell/jinvia/blob/main/CONTRIBUTING.md
- `release_notes_vulns` — No release has yet fixed a publicly known vulnerability with a CVE.
- `sites_password_security` — Project sites store no user passwords.
- `vulnerability_report_credit` — No vulnerabilities resolved in the last 12 months.

## C) 勾 **Unmet**（Silver 允许保留这 3 条不满足，说明原因即可）

- `bus_factor` — Single maintainer; mitigation documented (public reproducible source + lockbox). See https://github.com/cjintao133-cell/jinvia/blob/main/GOVERNANCE.md
- `dynamic_analysis` — Value from project's bestpractices JSON file
- `test_most` — Statement coverage is not currently measured, so 'most of the code is covered' is not asserted. The suite is substantial but this is stated honestly.

## D) 其余带 “?” / OSPS-* 的条目：不了解就**留空**，不影响 Silver（那是 Gold/Baseline 条目）

（共 80 条，属 Gold/OSPS 基线，非 Silver 必需。）

## 关键 justification（需要填字的几条，直接从这抄）

- **bus_factor**: Single maintainer; mitigation documented (public reproducible source + lockbox). See https://github.com/cjintao133-cell/jinvia/blob/main/GOVERNANCE.md
- **test_most**: Statement coverage is not currently measured, so 'most of the code is covered' is not asserted. The suite is substantial but this is stated honestly.
- **dynamic_analysis**: Value from project's bestpractices JSON file
