# 沙箱对抗演示（Sandbox Showdown）

这里没有 hello world，只有 **7 个恶意能力**，每一个都想搞死宿主：

| 恶意能力 | 攻击意图 | 拦在哪 | 退出码 |
|---|---|---|---|
| `a1-unauthorized` | 申请不存在的「写磁盘」本事 | 加载期（权限闸） | 3 |
| `a2-host-escape` | 偷调第 9 号宿主能力（只注册了 4 个） | 加载期（HOST 索引校验） | 3 |
| `a3-divzero` | 命令虚拟机算 `100 / 0` | 运行期（算术守卫） | 4 |
| `a4-sigfpe` | `INT64_MIN / -1`（朴素实现会 SIGFPE 崩进程） | 运行期（算术守卫） | 4 |
| `a5-oob-read` | 从线性内存地址 `-1` 读数据 | 运行期（内存边界检查） | 4 |
| `a6-infinite` | `JMP` 自环死循环 | 运行期（步数预算） | 4 |
| `a7-recursion` | 无底自我调用 | 运行期（帧数预算） | 4 |

跑一遍：

```bash
bash examples/sandbox-showdown/showdown.sh          # 讲故事（彩色）
bash examples/sandbox-showdown/showdown.sh --check   # CI 模式：全部被拦才 0 退出
```

它想搞死宿主，宿主连眉头都不皱 —— 这就是「默认拒绝 + 资源预算 + 边界检查」三根柱子的现场版。
每个 `main.jcbasm` 都是**手写**的恶意字节码，你可以直接 `./jinvia dis` 反汇编审计它们。
