/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jcb.h"
#include <stdlib.h>
#include <string.h>

static int has_operand(uint8_t op) {
    switch (op) {
        case OP_PUSH_C: case OP_LGET: case OP_LSET:
        case OP_JMP: case OP_JZ: case OP_JNZ:
        case OP_CALL: case OP_HOST:
        case OP_G_GET: case OP_G_SET: return 1;
        default: return 0;
    }
}
static int is_known_noarg(uint8_t op) {
    switch (op) {
        case OP_NOP: case OP_DUP: case OP_DROP:
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
        case OP_AND: case OP_OR: case OP_XOR: case OP_SHL: case OP_SHR: case OP_NOT:
        case OP_SWAP: case OP_OVER:
        case OP_EQ: case OP_LT: case OP_GT: case OP_LE: case OP_GE:
        case OP_RET: case OP_HALT:
        case OP_MEM_LOAD: case OP_MEM_STORE: case OP_MEM_LOAD8: case OP_MEM_STORE8: case OP_MEM_SIZE:
        case OP_MEM_LOAD32: case OP_MEM_STORE32: case OP_MEM_FILL: case OP_MEM_COPY: return 1;
        case OP_F64_ADD: case OP_F64_SUB: case OP_F64_MUL: case OP_F64_DIV:
        case OP_F64_NEG: case OP_F64_ABS:
        case OP_F64_EQ: case OP_F64_LT: case OP_F64_GT: case OP_F64_LE: case OP_F64_GE:
        case OP_I64_TO_F64: case OP_F64_TO_I64:
        case OP_F32_ADD: case OP_F32_SUB: case OP_F32_MUL: case OP_F32_DIV:
        case OP_F32_TO_F64: case OP_F64_TO_F32: case OP_I64_TO_F32: case OP_F32_TO_I64: return 1;
        default: return 0;
    }
}

int j_decode(const uint8_t *code, uint32_t code_len, JInstr **out, uint32_t *nout) {
    if ((!code && code_len) || !out || !nout) return J_ERR_FMT;
    *out = NULL; *nout = 0;
    JInstr *arr = NULL; uint32_t n = 0, cap = 0;

    uint32_t i = 0;
    while (i < code_len) {
        uint8_t op = code[i++];
        JInstr ins; ins.op = op; ins.arg = 0;
        if (has_operand(op)) {
            if (code_len - i < 4) { free(arr); return J_ERR_EOF; }
            ins.arg = (uint32_t)code[i] | ((uint32_t)code[i+1] << 8)
                    | ((uint32_t)code[i+2] << 16) | ((uint32_t)code[i+3] << 24);
            i += 4;
        } else if (!is_known_noarg(op)) {
            free(arr); return J_ERR_FMT;   /* 未知指令 */
        }
        if (n == cap) {
            uint32_t nc = cap ? cap * 2 : 16;
            JInstr *t = (JInstr *)realloc(arr, (size_t)nc * sizeof(JInstr));
            if (!t) { free(arr); return J_ERR_OOM; }
            arr = t; cap = nc;
        }
        arr[n++] = ins;
    }
    *out = arr; *nout = n; return J_OK;
}

int j_validate_func(const JModule *m, uint32_t func_index, const JInstr *ins, uint32_t n) {
    if (!m || func_index >= m->nfuncs) return J_ERR_FMT;
    if (n && !ins) return J_ERR_FMT;
    uint32_t local_slots = m->funcs[func_index].nparams + m->funcs[func_index].nlocals;

    for (uint32_t i = 0; i < n; i++) {
        uint8_t op = ins[i].op; uint32_t a = ins[i].arg;
        switch (op) {
            case OP_PUSH_C: if (a >= m->nconsts) return J_ERR_FMT; break;
            case OP_LGET: case OP_LSET: if (a >= local_slots) return J_ERR_FMT; break;
            case OP_CALL: if (a >= m->nfuncs) return J_ERR_FMT; break;
            case OP_G_GET: case OP_G_SET: if (a >= m->nglobals) return J_ERR_FMT; break;
            case OP_JMP: case OP_JZ: case OP_JNZ: if (a >= n) return J_ERR_FMT; break;
            case OP_HOST: /* 宿主函数表在运行时决定，静态只接受索引 */ break;
            default: break;
        }
    }
    return J_OK;
}
