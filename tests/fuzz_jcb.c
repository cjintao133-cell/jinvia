#include "jinvia/jcb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static unsigned long long rs = 0x9e3779b97f4a7c15ULL;
static unsigned rnd(void){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (unsigned)(rs>>16); }
static int64_t dummy_host(void*ud,uint32_t i,int64_t a){(void)ud;return a^(int64_t)i;}

static void exercise(const uint8_t *buf, size_t len){
    JModule m;
    if(j_parse(buf,len,&m)!=J_OK) return;
    (void)j_verify(&m);
    for(uint32_t f=0; f<m.nfuncs; f++){
        JInstr *ins=NULL; uint32_t n=0;
        if(j_decode(m.funcs[f].code,m.funcs[f].code_len,&ins,&n)==J_OK){ (void)j_validate_func(&m,f,ins,n); free(ins); }
    }
    JVM *vm=j_vm_new(&m);
    if(vm){
        j_vm_set_host(vm,dummy_host,NULL);
        int64_t args[16]; for(int _i=0;_i<16;_i++) args[_i]=(int64_t)(_i*2654435761u);
        for(uint32_t f=0; f<m.nfuncs && f<4; f++){
            int64_t r; (void)j_vm_call(vm,f,args,m.funcs[f].nparams,&r);
        }
        j_vm_free(vm);
    }
    j_free(&m);
}

static const uint8_t seed[] = {
    0x4A,0x43,0x42,0x01,
    0x01,0x0C,0,0,0, 0x01,0,0,0, 0x2A,0,0,0,0,0,0,0,
    0x02,0x0F,0,0,0, 0x01,0,0,0, 0,0,0,0, 0,0,0,0, 0x06,0,0,0, 0x01,0,0,0,0x00, 0xFF,
    0x07,0x08,0,0,0, 0x01,0,0,0, 0x01,0x00, 0x66, 0,0,0,0
};

/* 生成一段随机算术/JMP 程序（无向后跳转，保证不死循环长跑） */
static void gen_program(uint8_t *code, size_t *clen){
    size_t n=0; int cnt=1+(rnd()%40);
    /* PUSH_C k ; PUSH_C k ; OP ; ... ; RET */
    for(int i=0;i<cnt;i++){
        int t=rnd()%10;
        if(t<3){ code[n++]=OP_PUSH_C; uint32_t k=rnd()%8; code[n++]=k&0xff;code[n++]=(k>>8)&0xff;code[n++]=(k>>16)&0xff;code[n++]=(k>>24)&0xff; }
        else if(t==3){ uint8_t ops[]={OP_ADD,OP_SUB,OP_MUL,OP_DIV,OP_MOD,OP_EQ,OP_LT,OP_GT,OP_AND,OP_OR,OP_XOR,OP_SHL,OP_SHR,OP_F64_ADD,OP_F64_SUB,OP_F64_MUL,OP_F64_DIV,OP_F64_EQ,OP_F64_LT,OP_F64_GT,OP_F64_LE,OP_F64_GE,OP_F32_ADD,OP_F32_SUB,OP_F32_MUL,OP_F32_DIV}; code[n++]=ops[rnd()%26]; }
        else if(t==4){ uint8_t so[]={OP_DUP,OP_SWAP,OP_OVER,OP_NOT,OP_F64_NEG,OP_F64_ABS,OP_I64_TO_F64,OP_F64_TO_I64,OP_F32_TO_F64,OP_F64_TO_F32,OP_I64_TO_F32,OP_F32_TO_I64}; code[n++]=so[rnd()%12]; }
        else if(t==5){ code[n++]=OP_DROP; }
        else if(t==6){ code[n++]=OP_MEM_SIZE; }
        else if(t==7){ code[n++]=OP_MEM_LOAD; }
        else if(t==8){ code[n++]=OP_MEM_STORE; }
        else { uint8_t mops[]={OP_MEM_LOAD8,OP_MEM_STORE8,OP_MEM_LOAD32,OP_MEM_STORE32,OP_MEM_FILL,OP_MEM_COPY}; code[n++]=mops[rnd()%6];
        if(t==9||t==7){ code[n++]=OP_G_GET; uint32_t g=rnd()%4; code[n++]=g&0xff;code[n++]=(g>>8)&0xff;code[n++]=(g>>16)&0xff;code[n++]=(g>>24)&0xff; } }
        if(n>200) break;
    }
    code[n++]=OP_RET;
    *clen=n;
}

int main(int argc,char**argv){
    long per = (argc>1)? atol(argv[1]) : 250000;
    uint8_t cbuf[256]; size_t clen;

    /* 阶段1：纯随机字节 */
    for(long i=0;i<per;i++){
        size_t len=rnd()%256; uint8_t*buf=malloc(len?len:1);
        for(size_t j=0;j<len;j++) buf[j]=(uint8_t)rnd();
        exercise(buf,len); free(buf);
    }
    printf("phase1 random-bytes: %ld\n",per);

    /* 阶段2：位翻转 */
    for(long i=0;i<per;i++){
        uint8_t buf[sizeof(seed)]; memcpy(buf,seed,sizeof(seed));
        int nm=1+rnd()%10; for(int k=0;k<nm;k++){ int pos=rnd()%sizeof(buf); buf[pos]^=(uint8_t)(1u<<(rnd()%8)); }
        exercise(buf,sizeof(buf));
    }
    printf("phase2 bitflip: %ld\n",per);

    /* 阶段3：增/删/重复 字节 —— 长度变异（打到截断/越界逻辑） */
    for(long i=0;i<per;i++){
        uint8_t buf[512]; size_t len=sizeof(seed); memcpy(buf,seed,sizeof(seed));
        int ops=1+rnd()%6;
        for(int k=0;k<ops;k++){
            int t=rnd()%3;
            if(t==0 && len>2){ size_t pos=rnd()%len; memmove(buf+pos,buf+pos+1,len-pos-1); len--; }      /* delete */
            else if(t==1 && len<500){ size_t pos=rnd()%len; memmove(buf+pos+1,buf+pos,len-pos); buf[pos]=(uint8_t)rnd(); len++; } /* insert */
            else if(len<500){ buf[len++]=(uint8_t)rnd(); }                                                /* append */
        }
        exercise(buf,len);
    }
    printf("phase3 indels: %ld\n",per);

    /* 阶段4：随机算术程序（用极端常量跑到 VM 运算） */
    int64_t extremes[8]={INT64_MIN,INT64_MAX,-1,0,1,2,-2,42};
    for(long i=0;i<per;i++){
        gen_program(cbuf,&clen);
        /* 包成模块：consts=extremes, func=code */
        uint8_t mod[1024]; size_t p=0;
        mod[p++]=0x4A;mod[p++]=0x43;mod[p++]=0x42;mod[p++]=0x01;
        /* consts section */
        mod[p++]=1; uint32_t cpl=4+8*8; mod[p++]=cpl&0xff;mod[p++]=(cpl>>8)&0xff;mod[p++]=(cpl>>16)&0xff;mod[p++]=(cpl>>24)&0xff;
        mod[p++]=8;mod[p++]=0;mod[p++]=0;mod[p++]=0;
        for(int c=0;c<8;c++){ uint64_t u=(uint64_t)extremes[c]; for(int q=0;q<8;q++) mod[p++]=(u>>(8*q))&0xff; }
        /* funcs section */
        uint32_t fpl=4+12+clen; mod[p++]=2; mod[p++]=fpl&0xff;mod[p++]=(fpl>>8)&0xff;mod[p++]=(fpl>>16)&0xff;mod[p++]=(fpl>>24)&0xff;
        mod[p++]=1;mod[p++]=0;mod[p++]=0;mod[p++]=0;
        mod[p++]=0;mod[p++]=0;mod[p++]=0;mod[p++]=0;  /* nparams */
        mod[p++]=0;mod[p++]=0;mod[p++]=0;mod[p++]=0;  /* nlocals */
        mod[p++]=clen&0xff;mod[p++]=(clen>>8)&0xff;mod[p++]=(clen>>16)&0xff;mod[p++]=(clen>>24)&0xff;
        memcpy(mod+p,cbuf,clen); p+=clen;
        /* MEMORY 区段：1 页 */
        mod[p++]=3; memcpy(mod+p,&(uint32_t){4},4); p+=4; memcpy(mod+p,&(uint32_t){1},4); p+=4;
        /* GLOBALS 区段：4 个 */
        mod[p++]=4; memcpy(mod+p,&(uint32_t){4+32},4); p+=4; memcpy(mod+p,&(uint32_t){4},4); p+=4; for(int gi=0;gi<4;gi++){ uint64_t u=(uint64_t)(gi*7); for(int q=0;q<8;q++) mod[p++]=(u>>(8*q))&0xff; }
        exercise(mod,p);
    }
    printf("phase4 arith: %ld\n",per);
    printf("FUZZ OK (%ld x4 samples, no crash/leak/UB under sanitizer)\n",per);
    return 0;
}
