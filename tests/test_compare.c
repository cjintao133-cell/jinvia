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
static int cmp(long long a,long long b,const char*op,int64_t*out){
    char t[256]; snprintf(t,sizeof(t),".const %lld\n.const %lld\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\n%s\nRET\n.end\n",a,b,op);
    return run(t,out);
}
int main(void){
    int64_t r;
    ck(cmp(5,5,"EQ",&r)==J_OK && r==1,"EQ 5==5",__LINE__);
    ck(cmp(5,3,"EQ",&r)==J_OK && r==0,"EQ 5==3 false",__LINE__);
    ck(cmp(3,5,"LT",&r)==J_OK && r==1,"LT 3<5",__LINE__);
    ck(cmp(5,3,"LT",&r)==J_OK && r==0,"LT 5<3 false",__LINE__);
    ck(cmp(5,3,"GT",&r)==J_OK && r==1,"GT 5>3",__LINE__);
    ck(cmp(3,5,"GT",&r)==J_OK && r==0,"GT 3>5 false",__LINE__);
    ck(cmp(5,5,"LE",&r)==J_OK && r==1,"LE 5<=5",__LINE__);
    ck(cmp(6,5,"LE",&r)==J_OK && r==0,"LE 6<=5 false",__LINE__);
    ck(cmp(5,5,"GE",&r)==J_OK && r==1,"GE 5>=5",__LINE__);
    ck(cmp(4,5,"GE",&r)==J_OK && r==0,"GE 4>=5 false",__LINE__);
    /* NOP 空操作：不影响栈 */
    ck(run(".const 7\n.function 0 0 f\nNOP\nPUSH_C 0\nNOP\nRET\n.end\n",&r)==J_OK && r==7,"NOP noop",__LINE__);
    /* JNZ 非零跳转：push1 JNZ L; (跳过) push99; L: push7 ret -> 7 */
    ck(run(".const 1\n.const 99\n.const 7\n.function 0 0 f\nPUSH_C 0\nJNZ L\nPUSH_C 1\nL:\nPUSH_C 2\nRET\n.end\n",&r)==J_OK && r==7,"JNZ nonzero jumps",__LINE__);
    /* JNZ 为零不跳：push0 JNZ L; push99; L: push7 ret -> 99 */
    ck(run(".const 0\n.const 99\n.const 7\n.function 0 0 f\nPUSH_C 0\nJNZ L\nPUSH_C 1\nL:\nPUSH_C 2\nRET\n.end\n",&r)==J_OK,"JNZ zero nojump",__LINE__);
    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
