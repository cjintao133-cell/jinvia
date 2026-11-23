/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jcb.h"
#include <stdlib.h>
#include <string.h>

/* 每条指令的栈效应：min=至少需要的栈元素；net=净增减；flow=后继类型 */
typedef struct { int min; int net; int term; } Eff; /* term:0=fallthrough,1=jmp,2=cond,3=ret/halt */

static Eff eff_of(const JModule *m, const JInstr *in) {
    Eff e = {0, 0, 0};
    switch (in->op) {
        case OP_NOP: e.min=0; e.net=0; break;
        case OP_PUSH_C: case OP_LGET: case OP_G_GET: case OP_MEM_SIZE:
            e.min=0; e.net=1; break;
        case OP_DUP: e.min=1; e.net=1; break;
        case OP_DROP: e.min=1; e.net=-1; break;
        case OP_SWAP: e.min=2; e.net=0; break;
        case OP_OVER: e.min=2; e.net=1; break;
        case OP_LSET: case OP_G_SET: e.min=1; e.net=-1; break;
        case OP_NOT: e.min=1; e.net=0; break;
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_AND: case OP_OR: case OP_XOR: case OP_SHL: case OP_SHR:
        case OP_EQ: case OP_LT: case OP_GT: case OP_LE: case OP_GE:
            e.min=2; e.net=-1; break;
        case OP_HOST: e.min=1; e.net=0; break;
        case OP_MEM_LOAD: case OP_MEM_LOAD8: case OP_MEM_LOAD32:
            e.min=1; e.net=0; break;
        case OP_MEM_STORE: case OP_MEM_STORE8: case OP_MEM_STORE32:
            e.min=2; e.net=-2; break;
        case OP_MEM_FILL: case OP_MEM_COPY: e.min=3; e.net=-3; break;
        case OP_F64_ADD: case OP_F64_SUB: case OP_F64_MUL: case OP_F64_DIV:
        case OP_F64_EQ: case OP_F64_LT: case OP_F64_GT: case OP_F64_LE: case OP_F64_GE:
        case OP_F32_ADD: case OP_F32_SUB: case OP_F32_MUL: case OP_F32_DIV:
            e.min=2; e.net=-1; break;
        case OP_F64_NEG: case OP_F64_ABS:
        case OP_I64_TO_F64: case OP_F64_TO_I64:
        case OP_F32_TO_F64: case OP_F64_TO_F32: case OP_I64_TO_F32: case OP_F32_TO_I64:
            e.min=1; e.net=0; break;
        case OP_JMP: e.min=0; e.net=0; e.term=1; break;
        case OP_JZ: case OP_JNZ: e.min=1; e.net=-1; e.term=2; break;
        case OP_CALL: {
            uint32_t ti = (in->arg < m->nfuncs) ? m->funcs[in->arg].nparams : 0;
            e.min=(int)ti; e.net=(int)(1 - ti); e.term=0; break;
        }
        case OP_RET: e.min=1; e.net=0; e.term=3; break;
        case OP_HALT: e.min=0; e.net=0; e.term=3; break;
        default: e.min=0; e.net=0; break;
    }
    return e;
}

static int verify_func(const JModule *m, uint32_t f, JInstr **pins, uint32_t *pn) {
    JInstr *ins; uint32_t n;
    if (j_decode(m->funcs[f].code, m->funcs[f].code_len, &ins, &n) != J_OK) return J_ERR_FMT;
    if (j_validate_func(m, f, ins, n) != J_OK) { free(ins); return J_ERR_FMT; }
    *pins = ins; *pn = n;
    if (n == 0) return J_ERR_FMT;               /* 空函数体非法 */

    int *h = (int *)malloc((size_t)n * sizeof(int));
    int *todo = (int *)malloc((size_t)n * sizeof(int));
    if (!h || !todo) { free(h); free(todo); return J_ERR_OOM; } /* ins 所有权已在 *pins，交调用者释放 */
    for (uint32_t i = 0; i < n; i++) h[i] = -1;   /* -1 = 未定义 */
    int tn = 0;
    h[0] = 0; todo[tn++] = 0;                      /* 入口栈高 0（参数在 locals） */
    int rc = J_OK;

    while (tn > 0) {
        int pc = todo[--tn];
        if (pc < 0 || (uint32_t)pc >= n) { rc = J_ERR_FMT; break; }
        Eff e = eff_of(m, &ins[pc]);
        if (h[pc] < e.min) { rc = J_ERR_FMT; break; }        /* 栈下溢 */
        int nh = h[pc] + e.net;
        if (nh < 0) { rc = J_ERR_FMT; break; }

        /* 计算后继 */
        int succ[2]; int ns = 0;
        if (e.term == 3) { /* RET/HALT 终止 */ }
        else if (e.term == 1) { succ[ns++] = (int)ins[pc].arg; }       /* JMP */
        else if (e.term == 2) { succ[ns++] = (int)ins[pc].arg;          /* 跳转目标 */
                                if ((uint32_t)(pc + 1) < n) succ[ns++] = pc + 1; }
        else { if ((uint32_t)(pc + 1) < n) succ[ns++] = pc + 1;
               else { rc = J_ERR_FMT; break; } }                        /* 落到末尾=悬空 */

        for (int k = 0; k < ns; k++) {
            int s = succ[k];
            if (s < 0 || (uint32_t)s >= n) { rc = J_ERR_FMT; break; }
            if (h[s] == -1) { h[s] = nh; todo[tn++] = s; }
            else if (h[s] != nh) { rc = J_ERR_FMT; break; }             /* 栈高不一致 */
        }
        if (rc != J_OK) break;
    }
    /* 检查函数确实以终止指令收尾：最后一个可达块必须 RET/HALT/JMP 收口。
       简化：要求第 n-1 条是 RET/HALT（我们编译器保证），否则 JMP 自环也允许。 */
    {
        Eff last = eff_of(m, &ins[n-1]);
        if (last.term == 0) rc = J_ERR_FMT;   /* 末尾悬空 */
    }
    free(h); free(todo);
    return rc;
}

int j_verify(const JModule *m) {
    if (!m) return J_ERR_FMT;
    for (uint32_t f = 0; f < m->nfuncs; f++) {
        JInstr *ins = NULL; uint32_t n = 0;
        int rc = verify_func(m, f, &ins, &n);
        free(ins);
        if (rc != J_OK) return rc;
    }
    return J_OK;
}
