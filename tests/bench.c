/* Jinvia 基准：把"极小极快"变成数字。
 * 输出：执行吞吐(MIPS / ns每指令)、fib(24) 耗时、加载耗时(µs/次)。
 * 纯 C11 + POSIX 时钟，零第三方依赖。 */
#define _POSIX_C_SOURCE 199309L
#include "jinvia/jcb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static double now(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec + ts.tv_nsec/1e9; }
static uint32_t entryof(JModule*m){ for(uint32_t i=0;i<m->nexports;i++) if(!strcmp(m->export_names[i],"main")) return m->export_idx[i]; return 0; }

/* 循环体每轮 17 条指令：acc += (i*3 - 2); i++ */
static const char* LOOP_ASM =
" .const 0\n.const 5000000\n.const 1\n.const 3\n.const 2\n"
".function 0 2 main\n"
"PUSH_C 0\nLSET 1\n"
"PUSH_C 0\nLSET 0\n"
"loop:\n"
"LGET 0\nPUSH_C 1\nLT\nJZ done\n"
"LGET 1\nLGET 0\nPUSH_C 3\nMUL\nPUSH_C 4\nSUB\nADD\nLSET 1\n"
"LGET 0\nPUSH_C 2\nADD\nLSET 0\n"
"JMP loop\n"
"done:\n"
"LGET 1\nRET\n"
".end\n";

static const char* FIB_ASM =
".const 2\n.const 1\n.const 0\n.const 24\n"
".function 1 0 -\n"
"LGET 0\nPUSH_C 0\nLT\nJZ L0\nLGET 0\nRET\nJMP L1\nL0:\nL1:\n"
"LGET 0\nPUSH_C 1\nSUB\nCALL 0\n"
"LGET 0\nPUSH_C 0\nSUB\nCALL 0\nADD\nRET\n"
"PUSH_C 2\nRET\n.end\n"
".function 0 0 main\nPUSH_C 3\nCALL 0\nRET\nPUSH_C 2\nRET\n.end\n";

#define ITERS 5000000u
#define INSTR_PER_ITER 17u

int main(void){
    /* ---- 1) 执行吞吐 ---- */
    uint8_t *lb=NULL; size_t ll=0;
    if(j_asm(LOOP_ASM,&lb,&ll)!=J_OK){ printf("asm fail\n"); return 1; }
    JModule lm; if(j_parse(lb,ll,&lm)!=J_OK){ printf("parse fail\n"); return 1; }
    free(lb);
    JVM *vm=j_vm_new(&lm);
    JLimits lim=j_vm_default_limits(); lim.max_steps=100000000ULL; j_vm_set_limits(vm,lim);
    int64_t r=0;
    double t0=now(); int lrc=j_vm_call(vm,entryof(&lm),NULL,0,&r); double t1=now();
    double dt=t1-t0;
    long long want=3LL*(long long)ITERS*(long long)(ITERS-1)/2 - 2LL*(long long)ITERS;
    if(lrc!=J_OK){ printf("!! loop rc=%d (未跑完，数字无效)\n",lrc); return 1; }
    if((long long)r!=want){ printf("!! loop 结果不符 got=%lld want=%lld\n",(long long)r,want); return 1; }
    double iters_per_sec = ITERS/dt;
    double instrs = (double)ITERS*INSTR_PER_ITER;
    printf("== 执行吞吐（循环体 %u 指令/轮 × %u 轮）==\n", INSTR_PER_ITER, ITERS);
    printf("   耗时 %.4f s | %.1f M iters/s | %.1f MIPS | %.2f ns/指令\n",
           dt, iters_per_sec/1e6, (instrs/dt)/1e6, 1e9/(instrs/dt));
    j_vm_free(vm); j_free(&lm);

    /* ---- 2) fib(24) 递归 ---- */
    uint8_t *fb=NULL; size_t fl=0;
    if(j_asm(FIB_ASM,&fb,&fl)!=J_OK){ printf("fib asm fail\n"); return 1; }
    JModule fm; if(j_parse(fb,fl,&fm)!=J_OK){ printf("fib parse fail\n"); return 1; }
    free(fb);
    JVM *fvm=j_vm_new(&fm);
    double f0=now(); int64_t fr=0; j_vm_call(fvm,entryof(&fm),NULL,0,&fr); double f1=now();
    printf("== fib(24) 递归 ==\n");
    printf("   结果 %lld | 耗时 %.4f s\n",(long long)fr,f1-f0);
    j_vm_free(fvm); j_free(&fm);

    /* ---- 3) 加载耗时（parse+verify+vm_new）---- */
    uint8_t *pb=NULL; size_t pl=0; j_asm(LOOP_ASM,&pb,&pl);
    int N=5000; double l0=now();
    for(int i=0;i<N;i++){
        JModule m; if(j_parse(pb,pl,&m)==J_OK){ j_verify(&m); JVM*v=j_vm_new(&m); j_vm_free(v); j_free(&m); }
    }
    double l1=now();
    printf("== 加载（parse+verify+建VM，%d 次）==\n",N);
    printf("   %.1f µs/次 | 字节码 %zu 字节\n", (l1-l0)/N*1e6, pl);
    free(pb);
    return 0;
}
