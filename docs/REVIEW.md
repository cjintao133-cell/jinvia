# Jinvia · 安全与健壮性自查报告（REVIEW.md）

> 目的：在项目对外推广前，先自己当最挑剔的审稿人，把真问题挖出来、修掉、并留下证据。
> 方法：通读全部执行路径源码 + 构造恶意字节码实跑（红队）+ 现有测试门禁（ASan/UBSan/Valgrind/Fuzz）。
> 结论先放：**发现并修复 1 个真实的沙箱越权漏洞**；加固 1 个 DoS 面；其余原本就做对的地方，如实确认。

---

## 一、已修复：沙箱「默认拒绝」被绕过（严重，已修）

**问题**：铁律 #1 声称「needs 里没声明的能力，运行时一个都不给」。但 `jinvia_load` 原先只校验两件事：
1. 能力声明的 `needs` 全部被宿主注册；
2. 字节码里每条 `HOST` 指令的**索引 < 已注册宿主数**。

它**没有校验**「`HOST n` 指向的那个宿主，是否在这个能力的 `needs` 里声明过」。

**后果（实测复现）**：一个能力 `needs: read:temp`，字节码却写 `HOST 3`（指向已注册的 `open:door`），
加载通过、运行时真的执行了 `open:door`（打印 `🔓 开门`，退出码 0）。**即：未声明的宿主能力可被索引直接调用，默认拒绝形同虚设。**

```
# examples/sneaky/capability.conf
name: sneaky
needs: read:temp            # 只声明了 read:temp
entry: main.jcb
# examples/sneaky/main.jcbasm
.function 0 0 sneaky
PUSH_C 0
HOST 3                      # 却调用 open:door（宿主注册顺序里 idx 3）
RET
```
修复前：`授权通过… [host] open:door -> 🔓 开门  结果 = 0`（越权成功）
修复后：`拒绝运行: HOST 索引越界  退出码=3`（被拦）

**修复**：`jinvia_load` 的 HOST 指令扫描增加一条——`HOST idx` 指向的宿主名，必须在该能力的 `needs` 中。
新增 `name_in_needs()`；`test-c.sh` 增加回归断言 `[OK] undeclared-host bypass denied(3)`；
新增 `examples/sneaky` 作为永久回归样例。

> 说明：项目原有的 `sandbox-showdown`（7/7 恶意能力拦截）覆盖了 SIGFPE/越界读写/死循环/递归爆栈等，
> 但**恰好没有覆盖「未声明宿主调用」这一类**——本报告补上。

---

## 二、已加固：单个工具可长时间占用 MCP server（DoS，已加固）

**问题**：MCP server 是单线程串行，一次只跑一个 tool。原先只有「步数上限」（默认 1000 万步），
没有「实际耗时上限」——不同指令开销差异大，步数 ≠ 时间；一个慢/恶意能力可以拖住整个 server。

**修复**：`j_vm_call` 每 4096 步检查一次墙钟时间，超过预算立即 `J_ERR_VM` 中止；
新增 `jinvia_set_time_budget()`；CLI/MCP 默认 **2 秒**预算。
**ABI 安全**：`JLimits` 结构**未改动**（预算存放在 JVM/JinviaRuntime 内部），不破坏外部并列初始化。

实测：步数上限调到 1 万亿后，死循环在 **~305ms** 被墙钟预算精确掐断（证明该控制独立生效）。

---

## 三、经严格核查后「确认无问题」的地方（曾被我误判，如实记录）

- **字节码索引越界**：`OP_CALL / OP_PUSH_C / OP_LGET / OP_GSET / OP_JMP` 的即时数在
  `j_validate_func` 里都做了范围校验；`enter()` 的 func 索引、VM 的 globals/consts 访问均被上游验证器保证。
- **内存越界**：`MEM_LOAD/STORE32/FILL/COPY` 全部对 `mem_size` 做了边界检查，负地址拒绝。
- **算术安全**：加减乘用无符号回绕规避有符号溢出 UB；除零、`INT64_MIN / -1` 都已防 SIGFPE。
- **MCP 字符串参数**：`run_tool` 把 string 参数按 (ptr,len) 两槽写入线性内存并校验空间，
  entry 形参数与参数槽数不符会报错 —— 参数通路是通的（曾误疑「string 进不了 VM」，已撤回）。
- **验证器**：对每个函数做栈高度数据流分析，检测栈下溢/悬空/跳转目标越界/栈高不一致。

---

## 四、如实声明的局限（非 bug，是现状）

- **MCU / 裸机**：同构是**设计目标**，尚未在真实 MCU 上验证；现存可移植性证据为 aarch64 交叉编译（ARM64 Linux，非 Cortex-M 裸机）。运行时用到少量 libc，"零依赖"指零**第三方**依赖。
- **执行模型**：受限 C 子集 → 整数 JCB 字节码（无指针/结构体），**不兼容 WASM**，不能跑任意 Python/JS。
- **并发**：单个 `JinviaRuntime` 同一时刻只执行一个调用（非并发/可重入），需并发请用多实例。
- **覆盖率**：core 约 80.7%（仍在提升）。
- **性能**：~270 MIPS 为解释器**微基准**（纯算术循环），真实能力含 host 调用/内存操作会明显更低。

---

## 五、验证状态

- `make test`（`test-c.sh`）：**C-GATE GREEN** —— 17 套件 ×（normal + ASan+UBSan + Valgrind）
  + CLI 集成（thermostat / evil 拒绝 / **sneaky 越权拒绝** / cc e2e / mcp e2e）+ sandbox-showdown 7/7 + Fuzz 250000×4，全绿。
- 以上修复均已合入 `main` 并带回归断言，防止回退。
