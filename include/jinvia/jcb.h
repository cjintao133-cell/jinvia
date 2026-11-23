/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JINVIA_JCB_H
#define JINVIA_JCB_H
/* Jinvia Capability Bytecode (JCB) v0 —— 纯 C、零依赖、固定宽度小端。
 * 砖1：读取层（文件头 + CONSTS + FUNCS(原始code) + EXPORTS）。越界一律负错误码。 */
#include <stdint.h>
#include <stddef.h>

typedef enum {
    J_OK = 0,
    J_ERR_EOF = -1, J_ERR_MAGIC = -2, J_ERR_VERSION = -3, J_ERR_SECTION = -4,
    J_ERR_LIMIT = -5, J_ERR_OOM = -6, J_ERR_FMT = -7, J_ERR_VM = -8,
} JErr;

enum {
    OP_NOP=0x00, OP_PUSH_C=0x01, OP_DUP=0x02, OP_DROP=0x03, OP_LGET=0x04, OP_LSET=0x05,
    OP_SWAP=0x06, OP_OVER=0x07,
    OP_ADD=0x10, OP_SUB=0x11, OP_MUL=0x12, OP_DIV=0x13, OP_MOD=0x14,
    OP_AND=0x15, OP_OR=0x16, OP_XOR=0x17, OP_SHL=0x18, OP_SHR=0x19, OP_NOT=0x1A,
    OP_EQ=0x20, OP_LT=0x21, OP_GT=0x22, OP_LE=0x23, OP_GE=0x24,
    OP_JMP=0x30, OP_JZ=0x31, OP_JNZ=0x32, OP_CALL=0x40, OP_RET=0x41, OP_HOST=0x50,
    OP_MEM_LOAD=0x60, OP_MEM_STORE=0x61, OP_MEM_LOAD8=0x62, OP_MEM_STORE8=0x63, OP_MEM_SIZE=0x64,
    OP_MEM_LOAD32=0x65, OP_MEM_STORE32=0x66, OP_MEM_FILL=0x67, OP_MEM_COPY=0x68,
    OP_G_GET=0x70, OP_G_SET=0x71,
    OP_F64_ADD=0x80, OP_F64_SUB=0x81, OP_F64_MUL=0x82, OP_F64_DIV=0x83,
    OP_F64_NEG=0x84, OP_F64_ABS=0x85,
    OP_F64_EQ=0x86, OP_F64_LT=0x87, OP_F64_GT=0x88, OP_F64_LE=0x89, OP_F64_GE=0x8A,
    OP_I64_TO_F64=0x8B, OP_F64_TO_I64=0x8C,
    OP_F32_ADD=0x90, OP_F32_SUB=0x91, OP_F32_MUL=0x92, OP_F32_DIV=0x93,
    OP_F32_TO_F64=0x94, OP_F64_TO_F32=0x95, OP_I64_TO_F32=0x96, OP_F32_TO_I64=0x97,
    OP_HALT=0xFF
};

#define J_MAX_CONSTS  4096
#define J_MAX_FUNCS   1024
#define J_MAX_EXPORTS 256
#define J_MAX_NAME    128
#define J_MAX_CODE    65536
#define J_MAX_PARAMS  16
#define J_MAX_LOCALS  1024
#define J_PAGE        4096
#define J_MAX_MEM_PAGES 16   /* 64KB 上限，嵌入式友好 */
#define J_MAX_GLOBALS  1024

typedef struct { uint32_t nparams, nlocals, code_len; uint8_t *code; } JFunc;

typedef struct {
    int64_t  *consts;   uint32_t nconsts;
    JFunc    *funcs;    uint32_t nfuncs;
    char     (*export_names)[J_MAX_NAME]; uint32_t *export_idx; uint32_t nexports;
    uint32_t mem_pages;   /* LINEAR MEMORY 大小（页），0=无内存 */
    int64_t  *globals; uint32_t nglobals;  /* 模块级可变全局变量的初始值 */
} JModule;

int  j_parse(const uint8_t *buf, size_t len, JModule *out);
void j_free(JModule *m);

/* 解码后的一条指令（固定宽度：op + 可选 u32 参数） */
typedef struct { uint8_t op; uint32_t arg; } JInstr;

/* 砖2：把函数原始代码解码成结构化指令。成功 0；未知指令/截断返回负码，不残留分配。 */
int  j_decode(const uint8_t *code, uint32_t code_len, JInstr **out, uint32_t *nout);

/* 砖2：静态校验一条已解码指令流的索引范围（常量/局部/函数/跳转目标）。成功 0，否则负码。 */
int  j_validate_func(const JModule *m, uint32_t func_index, const JInstr *ins, uint32_t n);

/* 静态校验器：对全模块做栈高度数据流分析——加载期即拒绝
 * 栈下溢/分支栈高不一致/函数末尾悬空 等坏字节码（不必等到运行时才崩）。 */
int  j_verify(const JModule *m);

/* 反汇编：把模块写��可读文本到 out（须足够大）。返回字节数或负错误码。 */
int  j_disasm(const JModule *m, char *out, size_t cap);

/* 砖3：执行机。HOST 指令调用宿主回调——这是沙箱边界，只服务被授权的宿主函数。
 * 回调约定：收到一个 i64 参数，返回一个 i64 结果。 */
typedef int64_t (*JHostFn)(void *ud, uint32_t host_idx, int64_t arg);
typedef struct JVM JVM;
typedef struct { uint64_t max_steps; uint32_t max_stack; uint32_t max_frames; } JLimits;
JVM *j_vm_new(const JModule *m);
void j_vm_free(JVM *vm);
void j_vm_set_host(JVM *vm, JHostFn fn, void *ud);
JLimits j_vm_default_limits(void);
void    j_vm_set_limits(JVM *vm, JLimits lim);   /* 宿主收紧资源预算；超容量自动钳制 */
/* 执行函数 func（参数 args/nargs），结果写入 *result。成功 J_OK。 */
int  j_vm_call(JVM *vm, uint32_t func, const int64_t *args, uint32_t nargs, int64_t *result);
/* 线性内存访问（供 MCP 参数注入等宿主侧使用）：返回字节大小 / 写入若干字节（越界返回 -1）。 */
uint32_t j_vm_mem_size(JVM *vm);
int      j_vm_mem_write(JVM *vm, uint32_t off, const void *src, uint32_t len);

/* 砖6：JCB 汇编器。把文本 (.jcbasm) 编成 JCB 字节码。
 * 成功 J_OK 且 *out_buf 由 malloc 分配（调用方 free），*out_len 为字节数。 */
int j_asm(const char *text, uint8_t **out_buf, size_t *out_len);

#endif
