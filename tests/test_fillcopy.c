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
    /* FILL: 用 7 填满前 4 字节，然后 MEM_LOAD32 应读回 0x07070707 */
    const char*fill=
      ".const 7\n.const 0\n.const 4\n.memory 1\n.function 0 0 f\n"
      "PUSH_C 1\n"      /* addr 0 */
      "PUSH_C 2\n"      /* len 4 */
      "PUSH_C 0\n"      /* byte 7 */
      "MEM_FILL\n"
      "PUSH_C 1\nMEM_LOAD32\nRET\n.end\n";
    ck(run(fill,&r)==J_OK && r==0x07070707,"FILL 4 bytes with 7 -> 0x07070707",__LINE__);

    /* FILL len=0 是空操作，不崩 */
    ck(run(".const 0\n.const 0\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 1\nPUSH_C 1\nMEM_FILL\nPUSH_C 1\nMEM_LOAD32\nRET\n.end\n",&r)==J_OK,"FILL len 0 ok",__LINE__);

    /* COPY: 先在 addr0 存 0x1122334455667788，拷贝到 addr8，再读 addr8 */
    const char*copy=
      ".const 1234605616436508552\n.const 0\n.const 8\n.const 8\n.memory 1\n.function 0 0 f\n"
      "PUSH_C 1\nPUSH_C 0\nMEM_STORE\n"      /* mem[0..8)=val */
      "PUSH_C 2\n"   /* dst 8 */
      "PUSH_C 1\n"   /* src 0 */
      "PUSH_C 3\n"   /* len 8 */
      "MEM_COPY\n"
      "PUSH_C 2\nMEM_LOAD\nRET\n.end\n";     /* load from addr8 */
    ck(run(copy,&r)==J_OK && (uint64_t)r==1234605616436508552ULL,"COPY 8 bytes to addr8",__LINE__);

    /* COPY 重叠安全（memmove语义）：addr0 写 8 字节，copy dst1 src0 len7，应正确 */
    ck(run(".const 0\n.const 8\n.const 1\n.const 7\n.memory 1\n.function 0 0 f\nPUSH_C 1\nPUSH_C 0\nMEM_STORE\nPUSH_C 2\nPUSH_C 1\nPUSH_C 3\nMEM_COPY\nPUSH_C 1\nMEM_LOAD\nRET\n.end\n",&r)==J_OK,"overlapping COPY ok (memmove)",__LINE__);

    /* FILL 越界：addr 4090 len 8 -> 4098 越界 -> VM err */
    ck(run(".const 4090\n.const 8\n.const 1\n.memory 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nPUSH_C 2\nMEM_FILL\nRET\n.end\n",&r)==J_ERR_VM,"FILL overflow -> err",__LINE__);
    /* FILL 负 addr */
    ck(run(".const 0\n.const 1\n.const 1\n.memory 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nSUB\nPUSH_C 2\nPUSH_C 1\nMEM_FILL\nRET\n.end\n",&r)==J_ERR_VM,"FILL neg addr -> err",__LINE__);
    /* COPY 越界 dst */
    ck(run(".const 4090\n.const 0\n.const 8\n.memory 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 1\nPUSH_C 2\nMEM_COPY\nRET\n.end\n",&r)==J_ERR_VM,"COPY dst overflow -> err",__LINE__);
    /* 无内存且真实访问(len1) -> err */
    ck(run(".const 1\n.function 0 0 f\nPUSH_C 0\nPUSH_C 0\nPUSH_C 0\nMEM_FILL\nRET\n.end\n",&r)==J_ERR_VM,"FILL no memory len1 -> err",__LINE__);
    /* 无内存 len0 -> 空操作，成功返回 0 */
    ck(run(".const 0\n.function 0 0 f\nPUSH_C 0\nPUSH_C 0\nPUSH_C 0\nMEM_FILL\nPUSH_C 0\nRET\n.end\n",&r)==J_OK && r==0,"FILL no memory len0 noop ok",__LINE__);

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
