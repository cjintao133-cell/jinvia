/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *mn(uint8_t op){
    switch(op){
        case OP_NOP:return "nop"; case OP_PUSH_C:return "push_c"; case OP_DUP:return "dup"; case OP_DROP:return "drop";
        case OP_LGET:return "lget"; case OP_LSET:return "lset"; case OP_SWAP:return "swap"; case OP_OVER:return "over";
        case OP_ADD:return "add"; case OP_SUB:return "sub"; case OP_MUL:return "mul"; case OP_DIV:return "div"; case OP_MOD:return "mod";
        case OP_AND:return "and"; case OP_OR:return "or"; case OP_XOR:return "xor"; case OP_SHL:return "shl"; case OP_SHR:return "shr"; case OP_NOT:return "not";
        case OP_EQ:return "eq"; case OP_LT:return "lt"; case OP_GT:return "gt"; case OP_LE:return "le"; case OP_GE:return "ge";
        case OP_JMP:return "jmp"; case OP_JZ:return "jz"; case OP_JNZ:return "jnz"; case OP_CALL:return "call"; case OP_RET:return "ret"; case OP_HALT:return "halt";
        case OP_HOST:return "host";
        case OP_MEM_LOAD:return "mem_load"; case OP_MEM_STORE:return "mem_store"; case OP_MEM_LOAD8:return "mem_load8"; case OP_MEM_STORE8:return "mem_store8";
        case OP_MEM_SIZE:return "mem_size"; case OP_MEM_LOAD32:return "mem_load32"; case OP_MEM_STORE32:return "mem_store32";
        case OP_MEM_FILL:return "mem_fill"; case OP_MEM_COPY:return "mem_copy";
        case OP_G_GET:return "g_get"; case OP_G_SET:return "g_set";
        default:return "?";
    }
}
static int has_arg(uint8_t op){
    switch(op){case OP_PUSH_C:case OP_LGET:case OP_LSET:case OP_JMP:case OP_JZ:case OP_JNZ:case OP_CALL:case OP_HOST:case OP_G_GET:case OP_G_SET:return 1;default:return 0;}
}

int j_disasm(const JModule *m, char *out, size_t cap){
    if(!m||!out) return J_ERR_FMT;
    size_t w=0;
    #define P(...) do{ if(w>=cap) return J_ERR_LIMIT; int _n=snprintf(out+w, cap-w, __VA_ARGS__); if(_n<0) return J_ERR_FMT; if((size_t)_n>=cap-w) return J_ERR_LIMIT; w+=(size_t)_n; }while(0)
    P("; Jinvia JCB disassembly\n");
    P("memory: %u pages (%u bytes)\n", m->mem_pages, m->mem_pages*4096);
    P("globals: %u\n", m->nglobals);
    P("consts: %u\n", m->nconsts);
    for(uint32_t i=0;i<m->nconsts;i++) P("  c%u = %lld\n", i, (long long)m->consts[i]);
    for(uint32_t i=0;i<m->nglobals;i++) P("  g%u = %lld\n", i, (long long)m->globals[i]);
    P("exports: %u\n", m->nexports);
    for(uint32_t i=0;i<m->nexports;i++) P("  \"%s\" -> func %u\n", m->export_names[i], m->export_idx[i]);
    for(uint32_t f=0; f<m->nfuncs; f++){
        P("\nfunc %u (nparams=%u nlocals=%u):\n", f, m->funcs[f].nparams, m->funcs[f].nlocals);
        JInstr *ins=NULL; uint32_t n=0;
        if(j_decode(m->funcs[f].code, m->funcs[f].code_len, &ins, &n)!=J_OK){ P("  <decode error>\n"); continue; }
        for(uint32_t k=0;k<n;k++){
            if(has_arg(ins[k].op)) P("  %3u: %-10s %u\n", k, mn(ins[k].op), ins[k].arg);
            else P("  %3u: %s\n", k, mn(ins[k].op));
        }
        free(ins);
    }
    #undef P
    return (int)w;
}
