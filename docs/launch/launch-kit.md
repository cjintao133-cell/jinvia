# 发布作战包（Launch Kit）

> 目标：先在英文技术社区拿真实反馈，再回中文社区讲。**先别急着喊"发布"**——先确认真实反应。
> 铁律：诚实。不写"第一个""世界最强"。说清"草案、未经外部审计、MCP 适配是 roadmap"。

## 一句话钩子（选你喜欢的那条）

- 中：**「它想搞死宿主，宿主连眉头都不皱。」**（配 7 个恶意能力被拦的演示）
- 中：**「别的运行时都踩着一块地板（Go/JVM/Python），Jinvia 自己浇地板：纯 C、零依赖，从一块芯片到一台服务器，同一套字节码。」**
- EN：**"Every other agent runtime stands on a floor. Jinvia pours its own: C11, zero deps, the same bytecode from an MCU to a server."**
- EN：**"It tries to kill the host. The host doesn't blink."**

## 顺序与时机

1. **GitHub 仓库 + README 先就位**（英文优先，中文留 `.zh.md`）。确认 `make && ./jinvia run examples/thermostat-guard` 一条龙能跑。
2. **录一段 20 秒终端 demo**：`showdown.sh` 跑 7 个恶意能力全被拦 + `jinvia cc fib.c` 现场编译。GIF 放 README 顶部。**没有画面就没有传播。**
3. **Hacker News（Show HN）**：用 `show-hn.md`。北京时间 **周二～周四 21:00–23:00**（≈美东上午）发。发完 10 分钟内自己回第一条作者评论。
4. **Reddit**：`r/C_Programming`（正文用下方草稿）、`r/embedded`（换角度：MCU 上跑沙箱插件）、`r/programming`（可选）。
5. **中文**：V2EX「分享创造」、即刻、朋友圈、微博。等英文那边有真实反馈后再发，能带截图/star 数更有说服力。

## Reddit r/C_Programming 草稿（英文）

**Title:** A pure-C, zero-dependency bytecode sandbox (~2k lines) — write plugins in a C subset, get a load-time verifier + DoS budgets for free

**Body:**
I built Jinvia, a tiny capability sandbox in pure C11 (no deps). You write a capability in a small C subset (`jinvia cc fib.c`), it compiles to a hand-rolled fixed-width stack bytecode, and a least-privilege VM runs it with a step/stack/frame budget. There's a load-time static verifier (stack-height dataflow) that rejects malformed bytecode before execution.

Why not WASM: implementing the WASM spec correctly is a huge surface for one person; a ~40-op stack machine I can audit end-to-end and keep ASan/Valgrind/fuzz-clean. Trade-off is no WASM ecosystem, which I'm upfront about.

I have a "sandbox showdown" with 7 malicious capabilities (div-by-zero, INT64_MIN/-1, OOB read, infinite loop, recursion bomb, host-index escape, privilege escalation) — all blocked, zero crashes. Tests: 16 suites × (normal, ASan/UBSan, Valgrind) + fuzzing.

Repo: https://github.com/cjintao133-cell/jinvia — feedback very welcome, especially on the bespoke-bytecode decision.

## Reddit r/embedded 草稿（英文）

**Title:** A dependency-free bytecode sandbox small enough for MCUs — update logic without reflashing

**Body:**
Sharing Jinvia: a pure-C, zero-dependency sandbox meant for constrained targets — the idea being you ship a *capability* (portable bytecode) to a finished device and change its logic without a full firmware reflash, with hard limits so a bad plugin can't wedge the device. No OS assumptions; a C compiler is the whole toolchain. Host callbacks are the only way a capability touches hardware, and they're default-deny.

Caveat: hardware cross-arch (ARM/32-bit) verified only at source level so far; I'd love pointers from embedded folks. Repo: https://github.com/cjintao133-cell/jinvia

## 中文 · V2EX「分享创造」草稿

**标题：** [分享创造] 用纯 C 写了个零依赖的沙箱引擎，给不可信插件/AI 工具当"地板"

**正文：**
最近一直在想一个问题：AI agent 和 MCP 开始到处调用第三方工具，可这些工具默认是不可信代码。常规解法是容器、microVM、或者 WebAssembly（微软的 Wassette 就是 Rust+Wasmtime 跑 MCP 工具）——都对，但都**重**，塞不进单片机、无 OS 设备、或纯 C 宿主。

所以我从最底下那层重新浇了块地板：**Jinvia**，纯 C11、零第三方依赖的能力沙箱。
- 自研固定宽度栈字节码（不是 WASM），加载期静态校验，宿主可设步数/栈/帧预算；
- 作者不用写汇编——有 C 子集编译器 `jinvia cc`，写 C 就能出能力包；
- 同一个能力包，从芯片到服务器同构跑。

最带感的演示：`showdown.sh` 里 7 个恶意能力（越权、偷调宿主、除零、INT64_MIN/-1、越界读、死循环、递归炸弹）**全部被当场拦下，宿主零崩溃**。一句话：它想搞死宿主，宿主连眉头都不皱。

诚实声明：还没接 MCP；没做外部安全审计；跨架构只在源码级验证过。想听大家拍砖，尤其"自研字节码是不是生态上的死路"。

仓库：https://github.com/cjintao133-cell/jinvia

## 发布前 checklist

- [ ] `make && ./jinvia run examples/thermostat-guard` 一条龙能跑（英文 README quickstart 对齐）
- [ ] `bash examples/cc/build.sh` 全绿；`bash examples/sandbox-showdown/showdown.sh` 7/7 blocked
- [ ] README 顶部有 20 秒 demo GIF
- [ ] `make test` 全绿（16 套 × 3 + fuzz）
- [ ] Show HN 作者第一条评论已准备好（show-hn.md 后半段）
- [ ] 全程诚实：草案 / 无外部审计 / MCP 是 roadmap / C 子集无数组指针
