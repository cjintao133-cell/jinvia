#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t H(void*ud,uint32_t i,int64_t a){(void)ud;(void)i;return a;}

/* 汇编文本并执行入口(0参)，返回 rc；out 拿结果 */
static int asm_run(const char*t,int64_t*out){
    uint8_t*buf=NULL; size_t len=0;
    if(j_asm(t,&buf,&len)!=J_OK) return -1000;
    JModule m;
    if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -1001; }
    free(buf);
    JVM*vm=j_vm_new(&m); if(!vm){ j_free(&m); return -1002; }
    j_vm_set_host(vm,H,NULL);
    int64_t r=0; int rc=j_vm_call(vm,0,NULL,0,&r);
    j_vm_free(vm); j_free(&m);
    if(out) *out=r;
    return rc;
}

int main(void){
    int64_t r; int rc;
    /* 1) i64 加法溢出回绕：INT64_MAX + 1 = INT64_MIN */
    rc=asm_run(".const 9223372036854775807\n.const 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nADD\nRET\n.end\n",&r);
    ck(rc==J_OK && r==(-9223372036854775807LL-1),"i64 add overflow wraps to INT64_MIN",__LINE__);

    /* 2) 乘法溢出回绕：INT64_MAX * 2 = -2 */
    rc=asm_run(".const 9223372036854775807\n.const 2\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nMUL\nRET\n.end\n",&r);
    ck(rc==J_OK && r==-2,"i64 mul overflow wraps to -2",__LINE__);

    /* 3) INT64_MIN / -1 必须被拦（否则 UB），返回错误不崩 */
    rc=asm_run(".const -9223372036854775808\n.const -1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nDIV\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"INT64_MIN / -1 blocked",__LINE__);

    /* 4) 负数取模：-7 % 3 = -1（C 语义） */
    rc=asm_run(".const -7\n.const 3\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nMOD\nRET\n.end\n",&r);
    ck(rc==J_OK && r==-1,"-7 % 3 == -1",__LINE__);

    /* 5) 递归自调用 -> 帧深超限，返回错误不崩（不栈溢出） */
    rc=asm_run(".function 0 0 f\nCALL 0\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"self-recursion blocked (frames)",__LINE__);

    /* 6) 无限循环 -> 步数上限，返回错误不崩 */
    rc=asm_run(".function 0 0 g\nloop:\nJMP loop\n.end\n",&r);
    ck(rc==J_ERR_VM,"infinite loop blocked (steps)",__LINE__);

    /* 7) 超大 export 名 -> 解析拒绝（name>=128） */
    {
        /* 手造一个 export name 长度 200 的模块：头 + EXPORTS(id7) */
        uint8_t buf[300]; int p=0;
        buf[p++]='J';buf[p++]='C';buf[p++]='B'; buf[p++]=1;
        buf[p++]=7; /* id */
        uint32_t plen=4+2+200+4; /* count + namelen + name + idx */
        buf[p++]=plen&0xff;buf[p++]=(plen>>8)&0xff;buf[p++]=(plen>>16)&0xff;buf[p++]=(plen>>24)&0xff;
        buf[p++]=1;buf[p++]=0;buf[p++]=0;buf[p++]=0; /* count=1 */
        buf[p++]=200;buf[p++]=0; /* namelen=200 */
        for(int i=0;i<200;i++) buf[p++]='x';
        buf[p++]=0;buf[p++]=0;buf[p++]=0;buf[p++]=0; /* idx */
        JModule m;
        ck(j_parse(buf,p,&m)==J_ERR_LIMIT,"oversized export name rejected",__LINE__);
    }

    /* 8) 除零 / 取模零 */
    rc=asm_run(".const 5\n.const 0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nDIV\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"div0 blocked",__LINE__);
    rc=asm_run(".const 5\n.const 0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nMOD\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"mod0 blocked",__LINE__);

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
