#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t host_temp(void*ud,uint32_t idx,int64_t a){ (void)ud;(void)a; return idx==0?34:0; }

/* 汇编文本 -> 跑入口函数(0参) -> 结果 */
static int asm_and_run(const char *text, int64_t expect){
    uint8_t *buf=NULL; size_t len=0;
    if(j_asm(text,&buf,&len)!=J_OK) return -100;
    JModule m;
    if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -101; }
    free(buf);
    JVM *vm=j_vm_new(&m);
    if(!vm){ j_free(&m); return -102; }
    j_vm_set_host(vm,host_temp,NULL);
    int64_t r=0; int rc=j_vm_call(vm,0,NULL,0,&r);
    j_vm_free(vm); j_free(&m);
    if(rc!=J_OK) return -200+rc;
    return (r==expect)?0:-300;
}

int main(void){
    /* 1) 累加 1..10 = 55 */
    const char *sum =
        ".const 0\n.const 10\n.const 1\n"
        ".function 0 2 sum\n"
        "loop:\n"
        "LGET 0\nPUSH_C 1\nLT\nJZ done\n"
        "LGET 0\nPUSH_C 2\nADD\nLGET 1\nADD\nLSET 1\n"
        "LGET 0\nPUSH_C 2\nADD\nLSET 0\n"
        "JMP loop\n"
        "done:\nLGET 1\nRET\n"
        ".end\n";
    ck(asm_and_run(sum,55)==0,"asm sum=55",__LINE__);

    /* 2) 温度报警（HOST0=read:temp 34 > 30 -> HOST1 alarm -> 1） */
    const char *thermo =
        ".const 30\n.const 0\n.const 1\n"
        ".function 0 0 t\n"
        "PUSH_C 1\nHOST 0\nPUSH_C 0\nGT\nDUP\nJZ done\nPUSH_C 2\nHOST 1\nDROP\n"
        "done:\nRET\n.end\n";
    ck(asm_and_run(thermo,1)==0,"asm thermostat=1",__LINE__);

    /* 3) 纯计算 6*7=42 */
    const char *mul =
        ".const 6\n.const 7\n"
        ".function 0 0 m\nPUSH_C 0\nPUSH_C 1\nMUL\nRET\n.end\n";
    ck(asm_and_run(mul,42)==0,"asm mul=42",__LINE__);

    /* 4) 未知助记符 -> 汇编失败 */
    { uint8_t *b1=NULL; size_t l1=0; ck(j_asm(".function 0 0 x\nFOOBAR\n.end\n",&b1,&l1)!=J_OK,"bad mnemonic",__LINE__); }
    /* 5) 未定义标签 -> 汇编失败 */
    { uint8_t *b2=NULL; size_t l2=0; ck(j_asm(".function 0 0 x\nJMP nowhere\n.end\n",&b2,&l2)!=J_OK,"undefined label",__LINE__); }

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
