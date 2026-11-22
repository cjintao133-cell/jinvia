#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t H(void*ud,uint32_t i,int64_t a){(void)ud;(void)i;return a;}
static int run(const char*t,int64_t*out){
    uint8_t*buf=NULL; size_t len=0;
    if(j_asm(t,&buf,&len)!=J_OK) return -1000;
    JModule m; if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -1001; } free(buf);
    if(j_verify(&m)!=J_OK){ j_free(&m); return -1002; }
    JVM*vm=j_vm_new(&m); if(!vm){ j_free(&m); return -1003; }
    j_vm_set_host(vm,H,NULL);
    int64_t r=0; int rc=j_vm_call(vm,0,NULL,0,&r);
    j_vm_free(vm); j_free(&m); if(out)*out=r; return rc;
}
static double bits2d(int64_t v){ double d; memcpy(&d,&v,8); return d; }
int main(void){
    int64_t r;
    /* f64 加法：1.5 + 2.25 = 3.75 -> F64_TO_I64 = 3 (trunc)；先验证位模式 */
    ck(run(".constf 1.5\n.constf 2.25\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_ADD\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.75,"f64 1.5+2.25=3.75",__LINE__);
    /* f64 减法 5.5-2.0=3.5 */
    ck(run(".constf 5.5\n.constf 2.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_SUB\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.5,"f64 5.5-2=3.5",__LINE__);
    /* f64 乘法 2.5*4=10 */
    ck(run(".constf 2.5\n.constf 4.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_MUL\nRET\n.end\n",&r)==J_OK && bits2d(r)==10.0,"f64 2.5*4=10",__LINE__);
    /* f64 除法 7/2=3.5 */
    ck(run(".constf 7.0\n.constf 2.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_DIV\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.5,"f64 7/2=3.5",__LINE__);
    /* 比较 */
    ck(run(".constf 1.5\n.constf 2.5\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_LT\nRET\n.end\n",&r)==J_OK && r==1,"f64 1.5<2.5",__LINE__);
    ck(run(".constf 3.0\n.constf 3.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_EQ\nRET\n.end\n",&r)==J_OK && r==1,"f64 3==3",__LINE__);
    /* i64 -> f64 -> i64 往返 */
    ck(run(".const 42\n.function 0 0 f\nPUSH_C 0\nI64_TO_F64\nF64_TO_I64\nRET\n.end\n",&r)==J_OK && r==42,"i64->f64->i64=42",__LINE__);
    /* f64->i64 截断：3.9 -> 3 */
    ck(run(".constf 3.9\n.function 0 0 f\nPUSH_C 0\nF64_TO_I64\nRET\n.end\n",&r)==J_OK && r==3,"f64 3.9->i64=3",__LINE__);
    /* 负数截断 -3.9 -> -3 */
    ck(run(".constf -3.9\n.function 0 0 f\nPUSH_C 0\nF64_TO_I64\nRET\n.end\n",&r)==J_OK && r==-3,"f64 -3.9->-3",__LINE__);
    /* f64->i64 溢出饱和：1e30 -> INT64_MAX */
    ck(run(".constf 1e30\n.function 0 0 f\nPUSH_C 0\nF64_TO_I64\nRET\n.end\n",&r)==J_OK && r==INT64_MAX,"f64 1e30 saturates MAX",__LINE__);
    /* NaN -> 0：0/0 */
    ck(run(".constf 0.0\n.constf 0.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_DIV\nF64_TO_I64\nRET\n.end\n",&r)==J_OK && r==0,"NaN->i64=0",__LINE__);
    /* f64 NEG / ABS */
    ck(run(".constf 3.5\n.function 0 0 f\nPUSH_C 0\nF64_NEG\nRET\n.end\n",&r)==J_OK && bits2d(r)==-3.5,"f64 neg",__LINE__);
    ck(run(".constf -3.5\n.function 0 0 f\nPUSH_C 0\nF64_ABS\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.5,"f64 abs",__LINE__);
    /* f32 算术：1.5+2.25=3.75 (f32) */
    ck(run(".constf32 1.5\n.constf32 2.25\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF32_ADD\nF32_TO_F64\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.75,"f32 1.5+2.25=3.75",__LINE__);
    /* f32->i64 */
    ck(run(".constf32 7.9\n.function 0 0 f\nPUSH_C 0\nF32_TO_I64\nRET\n.end\n",&r)==J_OK && r==7,"f32 7.9->7",__LINE__);
    /* i64->f32->f64 */
    ck(run(".const 100\n.function 0 0 f\nPUSH_C 0\nI64_TO_F32\nF32_TO_F64\nRET\n.end\n",&r)==J_OK && bits2d(r)==100.0,"i64->f32->f64=100",__LINE__);
    /* 校验：F64_ADD 空栈应被 verify 拒 */
    /* 补齐其余浮点指令 */
    ck(run(".constf 3.0\n.constf 2.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_GT\nRET\n.end\n",&r)==J_OK && r==1,"F64_GT",__LINE__);
    ck(run(".constf 2.0\n.constf 3.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_LE\nRET\n.end\n",&r)==J_OK && r==1,"F64_LE",__LINE__);
    ck(run(".constf 3.0\n.constf 3.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF64_GE\nRET\n.end\n",&r)==J_OK && r==1,"F64_GE",__LINE__);
    ck(run(".constf32 5.5\n.constf32 2.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF32_SUB\nF32_TO_F64\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.5,"F32_SUB",__LINE__);
    ck(run(".constf32 2.5\n.constf32 4.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF32_MUL\nF32_TO_F64\nRET\n.end\n",&r)==J_OK && bits2d(r)==10.0,"F32_MUL",__LINE__);
    ck(run(".constf32 7.0\n.constf32 2.0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nF32_DIV\nF32_TO_F64\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.5,"F32_DIV",__LINE__);
    ck(run(".constf 3.5\n.function 0 0 f\nPUSH_C 0\nF64_TO_F32\nF32_TO_F64\nRET\n.end\n",&r)==J_OK && bits2d(r)==3.5,"F64_TO_F32 roundtrip",__LINE__);
    /* 浮点运算空栈 -> 运行/校验拒 */
    ck(run(".function 0 0 f\nF64_ADD\nRET\n.end\n",&r)==-1002,"verify f64 underflow",__LINE__);


    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
