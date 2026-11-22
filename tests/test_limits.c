#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t H(void*ud,uint32_t i,int64_t a){(void)ud;(void)i;return a;}

/* 汇编并执行；若 lim!=NULL 则施加该预算 */
static int run(const char*t, const JLimits*lim, int64_t*out){
    uint8_t*buf=NULL; size_t len=0;
    if(j_asm(t,&buf,&len)!=J_OK) return -1000;
    JModule m; if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -1001; } free(buf);
    JVM*vm=j_vm_new(&m); if(!vm){ j_free(&m); return -1002; }
    j_vm_set_host(vm,H,NULL);
    if(lim) j_vm_set_limits(vm,*lim);
    int64_t r; int rc=j_vm_call(vm,0,NULL,0,&r);
    j_vm_free(vm); j_free(&m);
    if(out) *out=r;
    return rc;
}

int main(void){
    int64_t r;
    /* 死循环：紧预算 -> 被掐；默认(10M) -> 也被掐；都不许挂 */
    const char*inf=".function 0 0 f\nloop:\nJMP loop\n.end\n";
    JLimits tight={1000,4096,256};
    ck(run(inf,&tight,&r)==J_ERR_VM,"infinite loop cut by tight step budget",__LINE__);
    ck(run(inf,NULL,&r)==J_ERR_VM,"infinite loop cut by default budget",__LINE__);

    /* 有限循环 sum 1..10=55：紧预算 1000 足够 -> 正常返回 55 */
    const char*sum=".const 0\n.const 10\n.const 1\n.function 0 2 s\nloop:\nLGET 0\nPUSH_C 1\nLT\nJZ done\nLGET 0\nPUSH_C 2\nADD\nLGET 1\nADD\nLSET 1\nLGET 0\nPUSH_C 2\nADD\nLSET 0\nJMP loop\ndone:\nLGET 1\nRET\n.end\n";
    ck(run(sum,&tight,&r)==J_OK && r==55,"finite loop completes under budget",__LINE__);

    /* 有限循环但预算太小(10步) -> 被掐，不返回错结果 */
    JLimits tiny={10,4096,256};
    ck(run(sum,&tiny,&r)==J_ERR_VM,"finite loop cut when budget too small",__LINE__);

    /* 深递归：默认帧256 -> 掐；紧帧8 -> 掐 */
    const char*rec=".function 0 0 f\nCALL 0\nRET\n.end\n";
    JLimits f8={10000000ULL,4096,8};
    ck(run(rec,&f8,&r)==J_ERR_VM,"deep recursion cut by frame budget",__LINE__);

    /* 爆栈：20次push，栈预算8 -> 掐 */
    {
        char prog[512]; int p=0;
        p+=sprintf(prog+p,".const 0\n.function 0 0 g\n");
        for(int i=0;i<20;i++) p+=sprintf(prog+p,"PUSH_C 0\n");
        p+=sprintf(prog+p,"RET\n.end\n");
        JLimits s8={10000000ULL,8,256};
        ck(run(prog,&s8,&r)==J_ERR_VM,"stack budget enforced",__LINE__);
        /* 同程序，宽栈 -> 能跑完（20个值，RET返回栈顶0） */
        ck(run(prog,NULL,&r)==J_OK && r==0,"same prog ok with default stack",__LINE__);
    }

    /* set_limits 钳制：传超大不崩，程序仍正常 */
    {
        JLimits huge={0,999999,999999};
        ck(run(sum,&huge,&r)==J_OK && r==55,"oversized limits clamped, still runs",__LINE__);
    }

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
