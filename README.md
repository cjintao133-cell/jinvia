[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/cjintao133-cell/jinvia/badge)](https://scorecard.dev/viewer/?uri=github.com/cjintao133-cell/jinvia)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/17017/badge)](https://www.bestpractices.dev/projects/17017)
[![CI](https://github.com/cjintao133-cell/jinvia/actions/workflows/ci.yml/badge.svg)](https://github.com/cjintao133-cell/jinvia/actions/workflows/ci.yml)
[![CodeQL](https://github.com/cjintao133-cell/jinvia/actions/workflows/codeql.yml/badge.svg)](https://github.com/cjintao133-cell/jinvia/actions/workflows/codeql.yml)
[![cjintao133-cell/jinvia MCP server](https://glama.ai/mcp/servers/cjintao133-cell/jinvia/badges/score.svg)](https://glama.ai/mcp/servers/cjintao133-cell/jinvia)

# Jinvia · 星垠

[English](README.en.md) · [中文](README.md)

> ### 一句话：给「MCP 工具 / AI 插件」用的**零依赖执行沙箱**
> 别人跑不可信工具，要拖 Rust / WASM 运行时 / 容器；Jinvia **只要一个 C 编译器**。
> 纯 C11、零第三方依赖、默认拒绝的能力沙箱，自带 MCP stdio server —— 把不受信的第三方工具关起来跑，而不用引入一整套重工具链。

**适合谁**：要把 MCP 工具 / AI 生成的插件安全跑起来、又**不想引 Rust/WASM/容器**的团队；纯 C / 嵌入式 / 无 OS / 边缘设备宿主（WASM 塞不进去的地方）。

**30 秒跑起来** —— 只需一个 C 编译器（gcc/clang），无其他依赖：
```bash
make
./jinvia run --trace examples/thermostat-guard
```
→ 读一次温度、超过 30 度触发报警，全程跑在「默认拒绝」的沙箱里。

![Jinvia sandbox showdown — 7/7 malicious capabilities blocked · fib(20)=6765](docs/assets/demo.gif)

> **无垠星河，无处不落。写一次，哪台机器都能跑。**
> An infinite river of stars — it lands on anything. Write once, run anywhere a C compiler reaches.

Jinvia 是一个**纯 C、零第三方依赖的 agent 内核**。能力（capability）以可移植字节码 **JCB** 分发，
跑在一套**最小权限、默认拒绝**的沙箱里。没有 Go runtime、没有 JVM、没有解释器要装——**一个 C 编译器就是全部前提**。

> 别的 agent 运行时都踩着一块地板（Go runtime / JVM / Python）。Jinvia 自己浇地板：
> C11、零依赖、从一块芯片到一台服务器同构运行。

---

## 为什么是现在：给 AI 工具一个「纯 C 的沙箱地板」

2025 年以来，MCP / AI agent 开始大规模调用**第三方工具**——而这些工具默认是**不受信任**的代码。
微软给出的答案是 [Wassette](https://github.com/microsoft/wassette)：用 WebAssembly 组件 + Wasmtime 把 MCP 工具关进沙箱。方向完全正确，但它**重**——要 Rust 工具链、要 WASM 组件生态，而且 Wasmtime 本身塞不进 MCU、无 OS 设备、或纯 C 宿主。

Jinvia 补的是同一块地板里**最底下那一层**：

- **纯 C11、零第三方依赖**：一个 C 编译器就是全部前提，没有 Rust / Go / JVM / WASM 运行时。
- **极小沙箱**：默认拒绝 + 资源预算 + 边界检查，恶意字节码当场被拦（见下方「沙箱对抗」）。
- **同构**：同一份能力包，从一块芯片到一台服务器跑的是同一套字节码。

> Jinvia 现在**自带一个最小 MCP stdio 服务器**（`jinvia mcp <工具目录>`）：每个能力包 = 一个 MCP tool，`tools/call` 时在沙箱里跑、默认拒绝。见 [`docs/mcp.md`](docs/mcp.md)。工具参数传递已支持：`capability.conf` 用 `params` 声明，`tools/list` 自动生成带类型（int/bool/string）的 `inputSchema`，`tools/call` 的 `arguments` 按名映射进 VM（string 经线性内存 ptr+len 传入）。仍在 roadmap：HTTP transport、resources/prompts、鉴权、可选参数/结构化结果。

---

## 性能与体量（实测）

> 环境：4C4G 云服务器 / gcc -O2 / 单线程。这是**解释器**（无 JIT）——追求"任何 C 编译器都能跑"，不是追求最快。数字用 `make bench` 复现。

| 指标 | 实测 |
|---|---|
| 执行吞吐 | **~270 MIPS**（≈ 3.7 ns/指令，循环算术体） |
| 递归 fib(24) | 46368，约 **9 ms** |
| 加载（parse+verify+建 VM） | **~0.9 µs/次**，模块字节码 184 字节 |
| VM 核心 `jcb_vm` | **~7 KB** text |
| 最小嵌入式内核（读+解码+VM） | **~10 KB** text |
| 全引擎 `.so`（含汇编器/反汇编/API） | **~52 KB** text |
| C 编译器 `jcb_cc` | ~18 KB（**仅编译期**需要，运行时可不链） |

> 对比参照：wasm3 约 8k star、内存 ~64KB 量级——同样走"极小解释器"路线。Jinvia 的取舍是**更极端的零依赖 + 自带 C 前端**，代价是不兼容 WASM（见下）。

## 快速开始

```bash
make                      # 只需一个 C 编译器
./jinvia asm examples/thermostat-guard/main.jcbasm examples/thermostat-guard/main.jcb
./jinvia run --trace examples/thermostat-guard
./jinvia dis examples/thermostat-guard/main.jcb   # 反汇编审计
```

```
载入能力: thermostat-guard —— 读一次温度，超过30度就报警
  申请本事(2): read:temp call:alarm
授权通过，执行 thermostat-guard ...
    [host] read:temp(入参 0) -> 34
    [host] call:alarm -> 🔔 报警!
结果 = 1
```

## 一个能力 = 两样东西

```
my-capability/
  capability.conf   # 声明：一行一条，纯文本，零依赖解析
  main.jcb          # 实现：JCB 字节码（用 `jinvia asm` 从 .jcbasm 汇编）
```

`capability.conf`：
```
name: thermostat-guard
does: 读一次温度，超过30度就报警
needs: read:temp, call:alarm
entry: main.jcb
```

## 换包即变 —— 同一台引擎，丢进不同包就变成不同工具

| 能力 | needs（申请的本事） | 运行结果 |
|---|---|---|
| `thermostat-guard` | read:temp, call:alarm | 读 34°C > 30 → 🔔 报警，返回 1 |
| `sum-1-to-10` | *(无，纯计算)* | 返回 55 |
| `sensor-double` | read:temp | 读 34 × 2 → 返回 68 |
| `door-guard` | read:motion, open:door | 感应有人 → 🔓 开门，返回 1 |
| `array-sum` | *(无，纯计算)* | 把 10,20,30 写进线性内存再求和 → 返回 60 |
| `evil`（反例） | read:temp, **write:disk** | **拒绝运行**（未授权，退出码 3） |

> 同一个内核，换个包就换个身份——**它是空白的，所以它什么都能是。**

> 注：CLI 的 `run` 子命令本身就**基于这套库**实现（`jinvia_register_host` 动态注册），不是另写一套。

## 用 C 写能力（不再手写汇编）

手写 `.jcbasm` 汇编是给引擎测试用的，不是给作者用的。给作者的是 **C 子集编译器**：

```bash
./jinvia cc  fib.c  fib.jcbasm      # C 子集 -> 汇编文本（可读、可审计）
./jinvia asm fib.jcbasm fib.jcb     # 汇编 -> 字节码
```

支持：`int` 全局/局部变量、函数（参数/返回值/递归）、`+ - * / %`、比较、`&& || !`、`if/else`、`while`、`return`，以及 `extern int host(int);` 映射到宿主能力。见 `examples/cc/`（`bash examples/cc/build.sh` 一键全编）：

| 程序 | C 源码 | 结果 |
|---|---|---|
| 斐波那契 | `fib(20)` 递归 | 6765 |
| 最大公约数 | `gcd(1071,462)` | 21 |
| 数素数 | `<100` 的素数个数 | 25 |
| 考拉兹 | `27` 到 1 的步数 | 111 |
| 传感器翻倍 | `read_temp(0)*2`（申请 `read:temp`） | 68 |

## 当库嵌入（把它装进你自己的程序）

```bash
make lib        # 产出 build/libjinvia.a 和 build/libjinvia.so
```

```c
#include "jinvia/jinvia.h"

int64_t my_read_temp(void *ud, int64_t arg) { return 34; }
int64_t my_alarm(void *ud, int64_t arg)     { /* 真报警 */ return 0; }

JinviaRuntime *rt = jinvia_new();
jinvia_register_host(rt, "read:temp",  my_read_temp, NULL);  // idx 0
jinvia_register_host(rt, "call:alarm", my_alarm,     NULL);  // idx 1
jinvia_set_limits(rt, 1000000, 4096, 256);                  // 资源预算
if (jinvia_load(rt, conf_text, jcb_bytes, jcb_len) == JINVIA_OK)
    jinvia_call(rt, &result);
jinvia_free(rt);
```

宿主**只注册自己愿意提供的功能**；能力申请了没注册的 needs → `JINVIA_ERR_NEEDS`（默认拒绝）。
完整示例见 `examples/embed/embed_demo.c`。

## 静态校验器 & 反汇编器

- **加载期静态校验**（`j_verify`）：对字节码做栈高度数据流分析，**加载时即拒绝**栈下溢、分支栈高不一致、函数末尾悬空等坏字节码——不必等运行到才崩。库与 CLI 加载能力时都会调用。
- **反汇编器**（`jinvia dis <file.jcb>`）：把字节码还原成可读文本，便于审计与调试。

## 安全模型（三根柱子）

1. **默认拒绝** —— `needs` 没声明的本事，运行时一个都不给；能力里若调用未声明的宿主函数，加载期直接拒。
2. **零共享** —— 能力与核不共享内存，只通过声明过的宿主函数说话。
3. **无处不落** —— 同一份能力包，从 MCU 到服务器同构运行。

**硬边界**（已实现并有测试）：
- DoS 预算：`JLimits{步数,栈,帧}` 可由宿主收紧，死循环/深递归/爆栈在预算内被掐，不挂。
- 算术：i64 加减乘按二补回绕（无 UB）；除/模拦截 `y==0` 与 `INT64_MIN/-1`（防 SIGFPE 崩进程）。
- 解析/校验期拒绝：超限字段、重复区段、未知指令、越界索引。


详见 [安全策略 SECURITY.md](SECURITY.md)（如何私下报告漏洞、范围、披露流程）。

## 沙箱对抗：它想搞死宿主，宿主连眉头都不皱

`examples/sandbox-showdown/` 里有 **7 个恶意能力**，各自想搞一种破坏：越权申请、偷调不存在的宿主、除零、`INT64_MIN/-1`（朴素实现会 SIGFPE 崩进程）、越界读、死循环、递归炸弹。

```bash
bash examples/sandbox-showdown/showdown.sh          # 讲故事（彩色）
bash examples/sandbox-showdown/showdown.sh --check   # CI 模式：7/7 被拦才 0 退出
```

结果：**全部当场被拦**——2 个在加载期被权限闸拒绝，5 个在运行期被算术 / 内存 / 预算守卫拦下，宿主零崩溃、零越权。这就是「默认拒绝 + 资源预算 + 边界检查」三根柱子的现场版。

## 当 MCP 服务器（给 AI 工具套沙箱）

Jinvia 自带一个**纯 C、零依赖的最小 MCP stdio 服务器**：把任意目录下的能力包暴露成 MCP tools，LLM 调用时在沙箱里执行其字节码。

```bash
./jinvia mcp examples          # 用 examples/ 当工具目录，通过 stdin/stdout 讲 MCP JSON-RPC
```

接到 Claude Desktop 等 MCP 客户端（配置里）：

```json
{ "mcpServers": { "jinvia": { "command": "/abs/path/jinvia", "args": ["mcp", "/abs/path/my-tools"] } } }
```

- `tools/list` → 每个能力包 = 一个 tool（name/description 来自 `capability.conf`）
- `tools/call` → 在沙箱里执行；**未在 `needs` 声明且宿主未注册的宿主能力 → 默认拒绝**（`isError:true`）
- 支持 `initialize` / `tools/list` / `tools/call` / `ping`；其余方法返回 `-32601`

> 关键区别：常规 MCP 服务器是**跑在宿主机上的原生二进制**，不可信工具≈宿主全权限；Jinvia 让工具作为**沙箱里的字节码**运行，只能通过宿主显式注册的回调碰硬件，且超预算即被掐。详见 [`docs/mcp.md`](docs/mcp.md)。

## JCB 字节码（自己的，不是 WASM）

固定宽度小端、~23 条 i64 栈机指令，解析器几十行纯 C 就能写。详见 [`SPEC_JCB.md`](SPEC_JCB.md)。
指令集：`PUSH_C/LGET/LSET/DUP/DROP/ADD/SUB/MUL/DIV/MOD/EQ/LT/GT/LE/GE/AND/OR/XOR/SHL/SHR/NOT/SWAP/OVER/JMP/JZ/JNZ/CALL/RET/HOST/HALT` + 线性内存 `MEM_LOAD/MEM_STORE/MEM_LOAD8/MEM_STORE8/MEM_LOAD32/MEM_STORE32/MEM_FILL/MEM_COPY/MEM_SIZE`（每次访问做边界检查）+ 全局变量 `G_GET/G_SET` + 浮点 `F64_ADD/SUB/MUL/DIV/NEG/ABS/CMP`、`I64_TO_F64/F64_TO_I64`、`F32_*`（f64/f32，转换饱和不 UB）。

### 为什么不用 WASM？

WASM 生态最大，但对这个项目是负担：完整实现 WASM 规范面太大，一个人很难同时做到"对 + 可审计 + ASan/Valgrind/fuzz 全绿"。Jinvia 用固定宽度 ~40 条栈指令，一个人能从读到尾。**代价很明确：不兼容 WASM，也没有现成的 WASM 工具链/包。** 如果你要的是 WASM 生态，直接用 wasm3 / WAMR —— 它们更适合。Jinvia 要的是"连 WASM 运行时都塞不进去的地方 + 自带 C 前端"。

## 现状（诚实版）

- ✅ **测试**：17 套单测（每套 normal + ASan/UBSan + Valgrind，含 C 子集编译器与 MCP 服务器各 3 连测试）、自定义多策略 fuzz（千万级样本）、
  libFuzzer 覆盖率引导（解析器 + 汇编器两路），CLI 端到端。四道门全绿。
- ✅ **已修的 4 个真安全洞**：重复区段内存泄漏、`INT64_MIN/-1` 触发 SIGFPE 崩进程、`nparams` 无上限致栈越界、内存越界检查的 `addr+8` 无符号溢出（负地址可绕过 → 越界读）。
- ✅ **源级可移植**：字符符号性矩阵（x86 有符号 / ARM 无符号）双过、`-Wcast-align=strict` / `-Wshadow` 全干净。
- ✅ **覆盖率**：核心运行时（VM/加载/校验/汇编，即执行不可信字节码的那层）**80.7%**（CI 硬门禁 ≥80%）；全树含 C 编译器与 MCP JSON 层 **75.5%**。
- ⚠️ **真机跨架构未验证**：ARM / 32 位“实际跑通”需在有交叉工具链 + QEMU 的机器上跑 `portability.sh` 第 3 步。
- ✅ **自带最小 MCP stdio 服务器**（tools/list+tools/call，默认拒绝）。
- ⚠️ **未经外部安全审计**：这是工作草案，别当作“production-grade”宣称。

## 目录

```
include/jinvia/   cap.h(能力声明) jcb.h(字节码+VM)
src/              cap / jcb_read / jcb_decode / jcb_vm / jcb_asm / jcb_cc(C编译器) / main
tests/            单测(6) + fuzz_jcb + fuzz_libfuzzer + fuzz_asm
examples/         thermostat-guard / sum-1-to-10 / sensor-double / door-guard / evil
                  + cc/（C 源码示例） + sandbox-showdown/（7 个恶意能力对抗）
SPEC.md          能力包格式   SPEC_JCB.md  字节码格式
test-c.sh        四道门总闸   portability.sh  可移植性检查   fuzz-soak.sh  长测
```

## License

[Apache-2.0](LICENSE) © Jinvia contributors. 一个 C 编译器就是全部前提——拿去用、改、嵌进闭源产品都行（保留版权与许可声明即可）。

---

> 💼 **需要商用支持 / 私有化加固部署 / 定制集成？** 开源核心 Apache-2.0 免费商用；我们提供付费的**加固镜像、私有部署、集成支持与 SLA**。
> 联系：**security@jinvia.dev**（或开一个带 `commercial` 标签的 issue）。详见 [`docs/TRUST.md`](docs/TRUST.md)。
