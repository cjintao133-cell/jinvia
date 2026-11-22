# Jinvia 字节码格式 v0 (JCB — Jinvia Capability Bytecode)

> 不是 WASM。是 Jinvia 自己的、纯 C 实现、极小的一套栈式字节码。
> 目标：几十条指令、固定宽度（无变长编码）、任何有 C 编译器的地方都能跑。

## 文件头
    "JCB"                3 字节魔数 (0x4A 0x43 0x42)
    version u8           = 0x01

## 区段（每个区段：id u8，size u32 小端，payload[size]）
    id=1 CONSTS   常量池：count u32，随后 count × i64（小端）
    id=2 FUNCS    函数：count u32，然后每函数 { nparams u32, nlocals u32, code_len u32, code[code_len] }
    id=7 EXPORTS  导出：count u32，然后每条 { namelen u16, name[], funcidx u32 }
    其它 id 一律按 size 跳过（向前兼容）

## 指令集 ISA（i64 栈机，全部固定宽度，操作数 u32 小端）
    0x00 NOP
    0x01 PUSH_C  k(u32)   ; 把 consts[k] 压栈
    0x02 DUP              ; 复制栈顶
    0x03 DROP             ; 丢弃栈顶
    0x04 LGET    i(u32)   ; 压入局部 i
    0x05 LSET    i(u32)   ; 弹出并写入局部 i
    0x10 ADD   0x11 SUB   0x12 MUL   0x13 DIV   0x14 MOD
    0x20 EQ    0x21 LT    0x22 GT    0x23 LE    0x24 GE   ; 结果为 0/1
    0x30 JMP   t(u32)     ; 跳到指令下标 t
    0x31 JZ    t(u32)     ; 弹栈；为 0 则跳到 t
    0x32 JNZ   t(u32)     ; 弹栈；非 0 则跳到 t
    0x40 CALL  f(u32)     ; 调用函数 f
    0x41 RET              ; 返回栈顶作为该函数返回值
    0x50 HOST  h(u32)     ; 调用宿主函数 h —— 沙箱边界：没授权的宿主函数一律拒绝
    0xFF HALT             ; 停机

## 为什么原创这套
- 固定宽度操作数 → 读取层简单到爆，纯 C 几十行，无变长编码坑。
- 只有 ~23 条指令 → 一个人能写全、能写对、能被 ASan/Valgrind 全量覆盖。
- 不和 wasm3/Wasmtime 抢赛道 —— 这是 Jinvia 自己的地板。

## 实现上限（安全护栏）
- 函数 nparams <= 16，nlocals <= 1024（解析期拒绝超限，防参数数组越界）
- 指令数/常量数/区段重复 均在解析或校验期拒绝
- 算术：i64 加减乘按二补回绕（无 UB）；除/模拦截 y==0 与 INT64_MIN/-1（防 SIGFPE）
