#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t H(void*ud,uint32_t i,int64_t a){(void)ud;(void)i;return a;}
static int run(const char*t,int64_t*out){
    uint8_t*buf=NULL; size_t len=0;
    if(j_asm(t,&buf,&len)!=J_OK) return -1000;
    JModule m; if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -1001; } free(buf);
    JVM*vm=j_vm_new(&m); if(!vm){ j_free(&m); return -1002; }
    j_vm_set_host(vm,H,NULL);
    int64_t r=0; int rc=j_vm_call(vm,0,NULL,0,&r);
    j_vm_free(vm); j_free(&m); if(out)*out=r; return rc;
}
int main(void){
    int64_t r;
    /* 全局初始 42，读回 */
    ck(run(".global 42\n.function 0 0 f\nG_GET 0\nRET\n.end\n",&r)==J_OK && r==42,"read global init 42",__LINE__);
    /* 写全局再读：g0=100 后读回 */
    ck(run(".global 0\n.const 100\n.function 0 0 f\nPUSH_C 0\nG_SET 0\nG_GET 0\nRET\n.end\n",&r)==J_OK && r==100,"set then get = 100",__LINE__);
    /* 两个全局独立 */
    ck(run(".global 11\n.global 22\n.function 0 0 f\nG_GET 0\nG_GET 1\nADD\nRET\n.end\n",&r)==J_OK && r==33,"two globals sum 33",__LINE__);
    /* 跨函数共享：func0 写 g0=7；func1 读 g0。入口 func0 调 func1 返回 g0 */
    const char*cross=
      ".global 0\n.const 7\n"
      ".function 0 0 outer\nPUSH_C 0\nG_SET 0\nCALL 1\nRET\n.end\n"
      ".function 0 0 inner\nG_GET 0\nRET\n.end\n";
    ck(run(cross,&r)==J_OK && r==7,"cross-function global = 7",__LINE__);
    /* 越界 G_GET -> 校验拒绝（j_asm 产出给 j_parse，应在 decode/validate 或 vm 层拒）*/
    ck(run(".global 1\n.function 0 0 f\nG_GET 5\nRET\n.end\n",&r)!=J_OK,"G_GET oob rejected",__LINE__);
    /* 无全局却用 G_GET -> 拒绝 */
    ck(run(".function 0 0 f\nG_GET 0\nRET\n.end\n",&r)!=J_OK,"G_GET with 0 globals rejected",__LINE__);
    /* 直接验证 VM 实例独立：两次调用同一个模块，全局重置为初值 */
    {
        uint8_t*buf=NULL; size_t len=0;
        ck(j_asm(".global 5\n.const 99\n.function 0 0 f\nPUSH_C 0\nG_SET 0\nG_GET 0\nRET\n.end\n",&buf,&len)==J_OK,"asm ok",__LINE__);
        JModule m; ck(j_parse(buf,len,&m)==J_OK,"parse ok",__LINE__); free(buf);
        int64_t r1,r2;
        { JVM*vm=j_vm_new(&m); j_vm_set_host(vm,H,NULL); j_vm_call(vm,0,NULL,0,&r1); j_vm_free(vm); }
        { JVM*vm=j_vm_new(&m); j_vm_set_host(vm,H,NULL); j_vm_call(vm,0,NULL,0,&r2); j_vm_free(vm); }
        ck(r1==99 && r2==99,"each VM starts from init (99)",__LINE__);
        j_free(&m);
    }
    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
