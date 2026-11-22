#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t H(void*ud,uint32_t i,int64_t a){(void)ud;(void)i;return a;}
static int asm_run(const char*t,int64_t*out){
    uint8_t*buf=NULL; size_t len=0;
    if(j_asm(t,&buf,&len)!=J_OK) return -1000;
    JModule m; if(j_parse(buf,len,&m)!=J_OK){ free(buf); return -1001; } free(buf);
    JVM*vm=j_vm_new(&m); if(!vm){ j_free(&m); return -1002; }
    j_vm_set_host(vm,H,NULL);
    int64_t r=0; int rc=j_vm_call(vm,0,NULL,0,&r);
    j_vm_free(vm); j_free(&m); if(out)*out=r; return rc;
}
static int binop(long long a,long long b,const char*op,int64_t*out){
    char t[256];
    snprintf(t,sizeof(t),".const %lld\n.const %lld\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\n%s\nRET\n.end\n",a,b,op);
    return asm_run(t,out);
}
int main(void){
    int64_t r;
    ck(binop(12,10,"AND",&r)==J_OK && r==(12&10),"AND",__LINE__);
    ck(binop(12,10,"OR",&r)==J_OK && r==(12|10),"OR",__LINE__);
    ck(binop(12,10,"XOR",&r)==J_OK && r==(12^10),"XOR",__LINE__);
    ck(binop(1,4,"SHL",&r)==J_OK && r==16,"SHL 1<<4",__LINE__);
    ck(binop(256,4,"SHR",&r)==J_OK && r==16,"SHR 256>>4",__LINE__);
    ck(binop(1,64,"SHL",&r)==J_OK && r==1,"SHL by 64 &63",__LINE__);
    ck(binop(1,65,"SHL",&r)==J_OK && r==2,"SHL by 65 &63",__LINE__);
    ck(binop(-1,1,"SHR",&r)==J_OK && (uint64_t)r==0x7FFFFFFFFFFFFFFFULL,"SHR logical -1",__LINE__);
    ck(asm_run(".const 0\n.function 0 0 f\nPUSH_C 0\nNOT\nRET\n.end\n",&r)==J_OK && r==-1,"NOT 0",__LINE__);
    ck(asm_run(".const 1\n.const 2\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nSWAP\nRET\n.end\n",&r)==J_OK && r==1,"SWAP",__LINE__);
    ck(asm_run(".const 1\n.const 2\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nOVER\nRET\n.end\n",&r)==J_OK && r==1,"OVER",__LINE__);
    ck(asm_run(".function 0 0 f\nSWAP\nRET\n.end\n",&r)==J_ERR_VM,"SWAP underflow",__LINE__);
    ck(asm_run(".function 0 0 f\nOVER\nRET\n.end\n",&r)==J_ERR_VM,"OVER underflow",__LINE__);
    ck(asm_run(".const 305419896\n.const 0\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE32\nPUSH_C 1\nMEM_LOAD32\nRET\n.end\n",&r)==J_OK && r==305419896,"MEM32 roundtrip",__LINE__);
    ck(asm_run(".const 305419896\n.const 0\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE32\nPUSH_C 1\nMEM_LOAD8\nRET\n.end\n",&r)==J_OK && r==0x78,"MEM_STORE32 low byte",__LINE__);
    ck(asm_run(".const 5\n.const 4092\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE32\nPUSH_C 1\nMEM_LOAD32\nRET\n.end\n",&r)==J_OK && r==5,"MEM32 at 4092",__LINE__);
    ck(asm_run(".const 5\n.const 4093\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE32\nRET\n.end\n",&r)==J_ERR_VM,"MEM32 at 4093 err",__LINE__);
    ck(asm_run(".const 0\n.const 1\n.memory 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nSUB\nMEM_LOAD32\nRET\n.end\n",&r)==J_ERR_VM,"MEM32 neg addr",__LINE__);
    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
