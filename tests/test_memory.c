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
    j_vm_free(vm); j_free(&m);
    if(out) *out=r;
    return rc;
}

int main(void){
    int64_t r; int rc;
    /* i64 存储/读取往返 */
    rc=asm_run(".const 42\n.const 0\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE\nPUSH_C 1\nMEM_LOAD\nRET\n.end\n",&r);
    ck(rc==J_OK && r==42,"i64 store/load roundtrip",__LINE__);

    /* 字节存储/读取（200 高位） */
    rc=asm_run(".const 200\n.const 0\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE8\nPUSH_C 1\nMEM_LOAD8\nRET\n.end\n",&r);
    ck(rc==J_OK && r==200,"byte store/load (200)",__LINE__);

    /* 写入 3 个 i64 再求和 = 60 */
    const char*sum=".const 10\n.const 20\n.const 30\n.const 0\n.const 8\n.const 16\n.memory 1\n.function 0 0 f\n"
      "PUSH_C 3\nPUSH_C 0\nMEM_STORE\nPUSH_C 4\nPUSH_C 1\nMEM_STORE\nPUSH_C 5\nPUSH_C 2\nMEM_STORE\n"
      "PUSH_C 3\nMEM_LOAD\nPUSH_C 4\nMEM_LOAD\nADD\nPUSH_C 5\nMEM_LOAD\nADD\nRET\n.end\n";
    ck(asm_run(sum,&r)==J_OK && r==60,"store 3x i64 and sum = 60",__LINE__);

    /* MEM_SIZE: .memory 1 -> 4096 */
    ck(asm_run(".memory 1\n.function 0 0 f\nMEM_SIZE\nRET\n.end\n",&r)==J_OK && r==4096,"MEM_SIZE==4096",__LINE__);
    /* .memory 2 -> 8192 */
    ck(asm_run(".memory 2\n.function 0 0 f\nMEM_SIZE\nRET\n.end\n",&r)==J_OK && r==8192,"MEM_SIZE==8192",__LINE__);

    /* 边界：i64 存到 4088 (4088+8=4096 恰好) OK */
    rc=asm_run(".const 7\n.const 4088\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE\nPUSH_C 1\nMEM_LOAD\nRET\n.end\n",&r);
    ck(rc==J_OK && r==7,"i64 at 4088 (last 8 bytes) OK",__LINE__);

    /* 边界：i64 存到 4090 (4090+8=4098 越界) -> VM 错误 */
    rc=asm_run(".const 7\n.const 4090\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE\nPUSH_C 1\nMEM_LOAD\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"i64 store at 4090 -> VM err",__LINE__);

    /* 边界：字节存到 4095 OK，4096 越界 */
    rc=asm_run(".const 1\n.const 4095\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE8\nPUSH_C 1\nMEM_LOAD8\nRET\n.end\n",&r);
    ck(rc==J_OK && r==1,"byte at 4095 OK",__LINE__);
    rc=asm_run(".const 1\n.const 4096\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE8\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"byte at 4096 -> VM err",__LINE__);

    /* 负地址 -> 越界。用算术造负数：0-1 = -1 */
    rc=asm_run(".const 0\n.const 1\n.memory 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nSUB\nMEM_LOAD8\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"negative addr -> VM err",__LINE__);

    /* 无 .memory 却用内存 -> VM 错误 */
    rc=asm_run(".const 0\n.function 0 0 f\nPUSH_C 0\nMEM_LOAD\nRET\n.end\n",&r);
    ck(rc==J_ERR_VM,"no memory declared -> VM err",__LINE__);
    ck(asm_run(".function 0 0 f\nMEM_SIZE\nRET\n.end\n",&r)==J_OK && r==0,"no memory -> MEM_SIZE 0",__LINE__);

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
