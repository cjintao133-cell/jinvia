#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }

int main(void){
    JInstr *ins=NULL; uint32_t n=0;

    /* --- 结构化解码 --- */
    /* PUSH_C 0 ; HALT */
    uint8_t c1[] = { OP_PUSH_C, 0x00,0x00,0x00,0x00, OP_HALT };
    ck(j_decode(c1,sizeof(c1),&ins,&n)==J_OK,"decode ok",__LINE__);
    ck(n==2 && ins[0].op==OP_PUSH_C && ins[0].arg==0 && ins[1].op==OP_HALT,"decode PUSH_C/HALT",__LINE__);
    free(ins); ins=NULL;

    /* 一串无参指令 */
    uint8_t c2[] = { OP_ADD,OP_SUB,OP_MUL,OP_DIV,OP_MOD,OP_DUP,OP_DROP,OP_RET };
    ck(j_decode(c2,sizeof(c2),&ins,&n)==J_OK && n==8 && ins[7].op==OP_RET,"decode noarg run",__LINE__);
    free(ins); ins=NULL;

    /* 带参指令参数正确（CALL 0x01020304） */
    uint8_t c3[] = { OP_CALL,0x04,0x03,0x02,0x01, OP_HALT };
    ck(j_decode(c3,sizeof(c3),&ins,&n)==J_OK && n==2 && ins[0].op==OP_CALL && ins[0].arg==0x01020304u,"decode CALL arg LE",__LINE__);
    free(ins); ins=NULL;

    /* 截断：PUSH_C 缺参数 */
    uint8_t c4[] = { OP_PUSH_C, 0x00,0x00 };
    ck(j_decode(c4,sizeof(c4),&ins,&n)==J_ERR_EOF,"decode truncated -> EOF",__LINE__);

    /* 未知指令 0x98（未分配）*/
    uint8_t c5[] = { OP_HALT, 0x98 };
    ck(j_decode(c5,sizeof(c5),&ins,&n)==J_ERR_FMT,"decode unknown op -> FMT",__LINE__);

    /* 空码 */
    ck(j_decode(NULL,0,&ins,&n)==J_OK && n==0,"decode empty ok",__LINE__);
    free(ins); ins=NULL;

    /* --- 范围校验（手工构造模块，栈上数据，不 j_free） --- */
    {
        static int64_t consts[2] = { 42, 7 };
        JFunc f; memset(&f,0,sizeof(f)); f.nparams=1; f.nlocals=1; /* local_slots=2 */
        JModule m; memset(&m,0,sizeof(m));
        m.consts=consts; m.nconsts=2; m.funcs=&f; m.nfuncs=1;

        JInstr ok[] = { {OP_PUSH_C,0}, {OP_LGET,1}, {OP_RET,0} };
        ck(j_validate_func(&m,0,ok,3)==J_OK,"validate valid ok",__LINE__);

        JInstr badc[] = { {OP_PUSH_C,5} };
        ck(j_validate_func(&m,0,badc,1)==J_ERR_FMT,"PUSH_C const oob",__LINE__);

        JInstr badl[] = { {OP_LGET,2} };
        ck(j_validate_func(&m,0,badl,1)==J_ERR_FMT,"LGET local oob (slots=2)",__LINE__);

        JInstr badcall[] = { {OP_CALL,1} };
        ck(j_validate_func(&m,0,badcall,1)==J_ERR_FMT,"CALL func oob",__LINE__);

        JInstr badjmp[] = { {OP_JMP,5} };
        ck(j_validate_func(&m,0,badjmp,1)==J_ERR_FMT,"JMP target oob",__LINE__);

        JInstr okjmp[] = { {OP_NOP,0}, {OP_JMP,0} };
        ck(j_validate_func(&m,0,okjmp,2)==J_OK,"JMP valid target ok",__LINE__);

        ck(j_validate_func(&m,1,ok,3)==J_ERR_FMT,"validate bad func index",__LINE__);
    }

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
