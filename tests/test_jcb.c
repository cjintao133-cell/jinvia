#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }

static const uint8_t good[] = {
    0x4A,0x43,0x42,0x01,0x01,0x0C,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x2A,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x02,0x16,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0xFF,
    0x07,0x0B,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x66,0x00,0x00,0x00,0x00,
};
static const uint8_t empty[] = {
    0x4A,0x43,0x42,0x01,
};
static const uint8_t bad_magic[] = {
    0x58,0x43,0x42,0x01,
};
static const uint8_t bad_ver[] = {
    0x4A,0x43,0x42,0x09,
};
static const uint8_t overrun[] = {
    0x4A,0x43,0x42,0x01,0x01,0x0F,0x27,0x00,0x00,
};

int main(void){
    JModule m;
    /* 完整解析 */
    ck(j_parse(good,sizeof(good),&m)==J_OK,"good parses",__LINE__);
    ck(m.nconsts==1 && m.consts[0]==42,"const 42",__LINE__);
    ck(m.nfuncs==1 && m.funcs[0].nparams==0 && m.funcs[0].nlocals==0,"func meta",__LINE__);
    ck(m.funcs[0].code_len==6 && m.funcs[0].code[0]==0x01 && m.funcs[0].code[5]==0xFF,"func code bytes",__LINE__);
    ck(m.nexports==1 && strcmp(m.export_names[0],"f")==0 && m.export_idx[0]==0,"export",__LINE__);
    j_free(&m);

    /* 空模块（只有头）合法 */
    ck(j_parse(empty,sizeof(empty),&m)==J_OK,"empty ok",__LINE__);
    ck(m.nconsts==0&&m.nfuncs==0&&m.nexports==0,"empty counts",__LINE__);
    j_free(&m);

    /* 错误路径 */
    ck(j_parse(bad_magic,sizeof(bad_magic),&m)==J_ERR_MAGIC,"bad magic",__LINE__);
    ck(j_parse(bad_ver,sizeof(bad_ver),&m)==J_ERR_VERSION,"bad version",__LINE__);
    ck(j_parse(overrun,sizeof(overrun),&m)==J_ERR_SECTION,"section overrun",__LINE__);
    { uint8_t t[2]={0x4A,0x43}; ck(j_parse(t,sizeof(t),&m)<0,"truncated",__LINE__); }
    ck(j_parse(NULL,0,&m)<0,"null buf",__LINE__);

    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
