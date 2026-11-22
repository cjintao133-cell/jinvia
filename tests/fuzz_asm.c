#include "jinvia/jcb.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int64_t dh(void*ud,uint32_t i,int64_t a){(void)ud;return a^(int64_t)i;}

/* libFuzzer：把任意字节当 .jcbasm 文本轰 j_asm；成功则回环 parse+run，验证不崩/不漏。 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size){
    uint8_t *text = malloc(size+1);
    if(!text) return 0;
    memcpy(text,data,size); text[size]='\0';
    uint8_t *buf=NULL; size_t len=0;
    if(j_asm((const char*)text,&buf,&len)==J_OK){
        JModule m;
        if(j_parse(buf,len,&m)==J_OK){
            JVM *vm=j_vm_new(&m);
            if(vm){
                j_vm_set_host(vm,dh,NULL);
                int64_t args[16]={0};
                for(uint32_t f=0; f<m.nfuncs && f<4; f++){ int64_t r; (void)j_vm_call(vm,f,args,m.funcs[f].nparams,&r); }
                j_vm_free(vm);
            }
            j_free(&m);
        }
        free(buf);
    }
    free(text);
    return 0;
}
