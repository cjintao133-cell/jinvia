# 转公开 & 点亮全部徽章 —— 操作手册

> 前提：你已确认要 public。这一步世界可见、基本不可逆。

## 1. 转 public
```bash
curl -s -X PATCH -H "Authorization: Bearer $(cat /root/.githubtoken)" \
  -H "Accept: application/vnd.github+json" \
  https://api.github.com/repos/cjintao133-cell/jinvia \
  -d '{"private":false}'
```

## 2. 启用 OpenSSF / CodeQL（公开后才可用）
```bash
cd /root/jinvia
cp docs/publish/scorecard.yml .github/workflows/scorecard.yml
cp docs/publish/codeql.yml    .github/workflows/codeql.yml
git add .github/workflows && git commit -m "ci: enable OpenSSF Scorecard + CodeQL" && git push
```
随后 README 徽章区加（示例）：
```
[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/cjintao133-cell/jinvia/badge)](https://scorecard.dev/viewer/?uri=github.com/cjintao133-cell/jinvia)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/<ID>/badge)](https://www.bestpractices.dev/projects/<ID>)
```

## 3. CII Best Practices
公开后到 https://www.bestpractices.dev 注册 `cjintao133-cell/jinvia`，按 `docs/openssf.md` 自评填问卷，拿 badge 粘到 README。

## 4. 收尾
- 仓库 About：已设 12 topics + 描述；建议补 homepage（可选）
- 打 release 已就绪（v0.1.0，prerelease）
- 发布文案：`docs/launch/show-hn.md`、`docs/launch/launch-kit.md`

## 5. 发布后自检
```bash
# 新克隆能否一条龙跑通（对应 CI 已有）
git clone https://github.com/cjintao133-cell/jinvia && cd jinvia && make && ./jinvia run examples/thermostat-guard
```
