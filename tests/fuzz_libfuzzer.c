#include "jinvia/jcb.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* libFuzzer 入口：喂进来的任意字节，走完整链路，绝不许崩/漏/UB。 */
static int64_t dummy_host(void*ud,uint32_t i,int64_t a){(void)ud;return a^(int64_t)i;}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size){
    JModule m;
    if(j_parse(data,size,&m)!=J_OK) return 0;
    (void)j_verify(&m);
    /* 对每个函数：解码+校验+执行（用极端参数） */
    for(uint32_t f=0; f<m.nfuncs && f<8; f++){
        JInstr *ins=NULL; uint32_t n=0;
        if(j_decode(m.funcs[f].code,m.funcs[f].code_len,&ins,&n)==J_OK){
            (void)j_validate_func(&m,f,ins,n);
            free(ins);
        }
    }
    JVM *vm=j_vm_new(&m);
    if(vm){
        j_vm_set_host(vm,dummy_host,NULL);
        int64_t args[16]={INT64_MIN,INT64_MAX,-1,0,1,2,-2,42,0,0,0,0,0,0,0,0};
        for(uint32_t f=0; f<m.nfuncs && f<8; f++){
            int64_t r; (void)j_vm_call(vm,f,args,m.funcs[f].nparams,&r);
        }
        j_vm_free(vm);
    }
    j_free(&m);
    return 0;
}
