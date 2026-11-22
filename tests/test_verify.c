#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }

/* 汇编文本 -> parse -> verify，返回 verify 结果 */
static int asm_verify(const char*t){
    uint8_t*buf=NULL; size_t len=0;
    if(j_asm(t,&buf,&len)!=J_OK) return -1000;
    JModule m; if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -1001; } free(buf);
    int rc=j_verify(&m); j_free(&m); return rc;
}
/* 校验一个示例包 */
static int verify_example(const char*dir){
    char jp[1024]; static unsigned char jcb[1<<20]; size_t jl; FILE*f;
    snprintf(jp,sizeof(jp),"%s/main.jcb",dir);
    f=fopen(jp,"rb"); if(!f) return -1; jl=fread(jcb,1,sizeof(jcb),f); fclose(f);
    JModule m; if(j_parse(jcb,jl,&m)!=J_OK) return -2;
    int rc=j_verify(&m); j_free(&m); return rc;
}
int main(void){
    /* 所有示例都必须通过校验 */
    const char*ex[]={"examples/thermostat-guard","examples/sum-1-to-10","examples/sensor-double","examples/door-guard","examples/array-sum","examples/evil"};
    for(int i=0;i<6;i++){ int rc=verify_example(ex[i]); if(rc!=J_OK) printf("  example %s rc=%d\n",ex[i],rc); ck(rc==J_OK,"example verifies",__LINE__); }

    /* 良好程序通过 */
    ck(asm_verify(".function 0 2 f\nLGET 0\nLGET 1\nADD\nRET\n.end\n")==J_OK,"balanced add passes",__LINE__);
    ck(asm_verify(".const 1\n.function 0 0 f\nPUSH_C 0\nRET\n.end\n")==J_OK,"const+ret passes",__LINE__);
    ck(asm_verify(".const 0\n.function 0 2 s\nloop:\nLGET 0\nPUSH_C 0\nLT\nJZ done\nLGET 1\nLGET 0\nADD\nLSET 1\nLGET 0\nPUSH_C 0\nADD\nLSET 0\nJMP loop\ndone:\nLGET 1\nRET\n.end\n")==J_OK,"loop passes",__LINE__);

    /* 栈下溢：空栈上 ADD */
    ck(asm_verify(".function 0 0 f\nADD\nRET\n.end\n")==J_ERR_FMT,"underflow ADD rejected",__LINE__);
    /* 栈下溢：DROP 空栈 */
    ck(asm_verify(".function 0 0 f\nDROP\nRET\n.end\n")==J_ERR_FMT,"underflow DROP rejected",__LINE__);
    /* 悬空：没有 RET/HALT 结尾 */
    ck(asm_verify(".function 0 0 f\nNOP\n.end\n")==J_ERR_FMT,"fall-off-end rejected",__LINE__);
    ck(asm_verify(".const 1\n.function 0 0 f\nPUSH_C 0\n.end\n")==J_ERR_FMT,"push-no-ret rejected",__LINE__);
    /* RET 时栈空 */
    ck(asm_verify(".function 0 0 f\nRET\n.end\n")==J_ERR_FMT,"RET with empty stack rejected",__LINE__);

    /* 分支栈高不一致：if 两路高度不同汇到一个块。
       构造：push1 JZ L ; pushval ; L: RET  —— 到达 L: 一路经 JNZ? 用 JZ:
       路径A: PUSH_C0(=1) JZ L (pop1->h0) -> L 高0
       路径B: 不跳则执行 PUSH_C1 (h1) 然后落到 L? 需顺序到 L。让 L 紧跟 push1:
       pc: 0 PUSH_C0 ; 1 JZ 3 ; 2 PUSH_C1 ; 3 L: RET。到 pc3: 路径A高0, 路径B高1 -> 不一致 */
    ck(asm_verify(".const 1\n.const 2\n.function 0 0 f\nPUSH_C 0\nJZ L\nPUSH_C 1\nL:\nRET\n.end\n")==J_ERR_FMT,"branch height mismatch rejected",__LINE__);

    /* 反汇编器：拆全部示例，均应含 "func" 与 "ret"（覆盖各指令助记符） */
    {
        const char*exd[]={"examples/thermostat-guard","examples/sum-1-to-10","examples/sensor-double","examples/door-guard","examples/array-sum"};
        for(int i=0;i<5;i++){
            char jp[1024]; static unsigned char jcb[1<<20]; size_t jl; FILE*f;
            snprintf(jp,sizeof(jp),"%s/main.jcb",exd[i]);
            f=fopen(jp,"rb"); if(!f) continue; jl=fread(jcb,1,sizeof(jcb),f); fclose(f);
            JModule m; if(j_parse(jcb,jl,&m)==J_OK){ static char out[262144];
                int n=j_disasm(&m,out,sizeof(out)-1);
                ck(n>0 && strstr(out,"func")!=NULL && strstr(out,"ret")!=NULL,"disasm contains func/ret",__LINE__);
                j_free(&m); }
        }
    }
    /* 反汇编错误路径：NULL / 太小缓冲 */
    {
        JModule m; memset(&m,0,sizeof(m));
        char out[8];
        ck(j_disasm(NULL, out, sizeof(out)) == J_ERR_FMT, "disasm null module", __LINE__);
        ck(j_disasm(&m, NULL, sizeof(out)) == J_ERR_FMT, "disasm null out", __LINE__);
        ck(j_disasm(&m, out, 2) == J_ERR_LIMIT, "disasm tiny buf -> LIMIT", __LINE__);
    }
    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
