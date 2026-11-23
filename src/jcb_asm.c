/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jcb.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static char *trim(char *s){ while(*s&&isspace((unsigned char)*s))s++; char*e=s+strlen(s); while(e>s&&isspace((unsigned char)e[-1]))*--e='\0'; return s; }
static void scpy(char *d, size_t cap, const char *src){ size_t n=strlen(src); if(n>=cap)n=cap-1; memcpy(d,src,n); d[n]='\0'; }

static int mnemonic(const char *mn, int *has_op){
    struct { const char *n; uint8_t op; int h; } T[] = {
        {"NOP",OP_NOP,0},{"DUP",OP_DUP,0},{"DROP",OP_DROP,0},
        {"ADD",OP_ADD,0},{"SUB",OP_SUB,0},{"MUL",OP_MUL,0},{"DIV",OP_DIV,0},{"MOD",OP_MOD,0},
        {"EQ",OP_EQ,0},{"LT",OP_LT,0},{"GT",OP_GT,0},{"LE",OP_LE,0},{"GE",OP_GE,0},
        {"AND",OP_AND,0},{"OR",OP_OR,0},{"XOR",OP_XOR,0},{"SHL",OP_SHL,0},{"SHR",OP_SHR,0},{"NOT",OP_NOT,0},
        {"SWAP",OP_SWAP,0},{"OVER",OP_OVER,0},
        {"F64_ADD",OP_F64_ADD,0},{"F64_SUB",OP_F64_SUB,0},{"F64_MUL",OP_F64_MUL,0},{"F64_DIV",OP_F64_DIV,0},
        {"F64_NEG",OP_F64_NEG,0},{"F64_ABS",OP_F64_ABS,0},
        {"F64_EQ",OP_F64_EQ,0},{"F64_LT",OP_F64_LT,0},{"F64_GT",OP_F64_GT,0},{"F64_LE",OP_F64_LE,0},{"F64_GE",OP_F64_GE,0},
        {"I64_TO_F64",OP_I64_TO_F64,0},{"F64_TO_I64",OP_F64_TO_I64,0},
        {"F32_ADD",OP_F32_ADD,0},{"F32_SUB",OP_F32_SUB,0},{"F32_MUL",OP_F32_MUL,0},{"F32_DIV",OP_F32_DIV,0},
        {"F32_TO_F64",OP_F32_TO_F64,0},{"F64_TO_F32",OP_F64_TO_F32,0},{"I64_TO_F32",OP_I64_TO_F32,0},{"F32_TO_I64",OP_F32_TO_I64,0},
        {"RET",OP_RET,0},{"HALT",OP_HALT,0},
        {"MEM_LOAD",OP_MEM_LOAD,0},{"MEM_STORE",OP_MEM_STORE,0},{"MEM_LOAD8",OP_MEM_LOAD8,0},{"MEM_STORE8",OP_MEM_STORE8,0},{"MEM_SIZE",OP_MEM_SIZE,0},
        {"MEM_LOAD32",OP_MEM_LOAD32,0},{"MEM_STORE32",OP_MEM_STORE32,0},{"MEM_FILL",OP_MEM_FILL,0},{"MEM_COPY",OP_MEM_COPY,0},
        {"PUSH_C",OP_PUSH_C,1},{"LGET",OP_LGET,1},{"LSET",OP_LSET,1},
        {"JMP",OP_JMP,1},{"JZ",OP_JZ,1},{"JNZ",OP_JNZ,1},{"CALL",OP_CALL,1},{"HOST",OP_HOST,1},
        {"G_GET",OP_G_GET,1},{"G_SET",OP_G_SET,1},
    };
    for(size_t i=0;i<sizeof(T)/sizeof(T[0]);i++) if(strcmp(mn,T[i].n)==0){ *has_op=T[i].h; return T[i].op; }
    return -1;
}
static int has_operand(uint8_t op){
    switch(op){ case OP_PUSH_C:case OP_LGET:case OP_LSET:case OP_JMP:case OP_JZ:case OP_JNZ:case OP_CALL:case OP_HOST:case OP_G_GET:case OP_G_SET: return 1; default: return 0; }
}

typedef struct { uint8_t op; long operand; int is_label; char label[64]; } AIns;
typedef struct { uint32_t nparams, nlocals; char export_name[J_MAX_NAME]; AIns *ins; uint32_t n, cap;
                 char (*labels)[64]; uint32_t *label_idx; uint32_t nlabels; } AFunc;

static void put_u32(uint8_t **p, uint32_t v){ (*p)[0]=(uint8_t)(v&0xff);(*p)[1]=(uint8_t)((v>>8)&0xff);(*p)[2]=(uint8_t)((v>>16)&0xff);(*p)[3]=(uint8_t)((v>>24)&0xff); *p+=4; }
static void put_u64(uint8_t **p, uint64_t v){ for(int i=0;i<8;i++) (*p)[i]=(uint8_t)((v>>(8*i))&0xff); *p+=8; }
static void put_u16(uint8_t **p, uint16_t v){ (*p)[0]=(uint8_t)(v&0xff);(*p)[1]=(uint8_t)((v>>8)&0xff); *p+=2; }

int j_asm(const char *text, uint8_t **out_buf, size_t *out_len){
    if(!text||!out_buf||!out_len) return J_ERR_FMT;
    *out_buf=NULL; *out_len=0;

    int64_t *consts=NULL; uint32_t nconsts=0, ccap=0;
    uint32_t mem_pages=0;
    int64_t *globals=NULL; uint32_t nglobals=0, gcap=0;
    AFunc *funcs=NULL; uint32_t nfuncs=0, fcap=0;
    size_t tlen=strlen(text);
    char *work=(char*)malloc(tlen+1); if(!work) return J_ERR_OOM;
    memcpy(work,text,tlen+1);
    int rc=J_OK; AFunc *cur=NULL;

    for(char *line=strtok(work,"\n"); line; line=strtok(NULL,"\n")){
        char *semi=strchr(line,';'); if(semi)*semi='\0';
        char *ln=trim(line); if(!*ln) continue;

        if(strncmp(ln,".constf32",9)==0){
            char *v=trim(ln+9); char *end=NULL; double d=strtod(v,&end);
            if(end==v){ rc=J_ERR_FMT; goto done; }
            float fv=(float)d; uint32_t u; memcpy(&u,&fv,4);
            if(nconsts==ccap){ uint32_t nc=ccap?ccap*2:8; int64_t*t=realloc(consts,nc*sizeof(int64_t)); if(!t){rc=J_ERR_OOM;goto done;} consts=t;ccap=nc; }
            consts[nconsts++]=(int64_t)(uint32_t)u;
        } else if(strncmp(ln,".constf",7)==0){
            char *v=trim(ln+7); char *end=NULL; double d=strtod(v,&end);
            if(end==v){ rc=J_ERR_FMT; goto done; }
            uint64_t u; memcpy(&u,&d,8);
            if(nconsts==ccap){ uint32_t nc=ccap?ccap*2:8; int64_t*t=realloc(consts,nc*sizeof(int64_t)); if(!t){rc=J_ERR_OOM;goto done;} consts=t;ccap=nc; }
            consts[nconsts++]=(int64_t)u;
        } else if(strncmp(ln,".const",6)==0){
            char *v=trim(ln+6); char *end=NULL; long long x=strtoll(v,&end,10);
            if(end==v){ rc=J_ERR_FMT; goto done; }
            if(nconsts==ccap){ uint32_t nc=ccap?ccap*2:8; int64_t*t=realloc(consts,nc*sizeof(int64_t)); if(!t){rc=J_ERR_OOM;goto done;} consts=t;ccap=nc; }
            consts[nconsts++]=(int64_t)x;
        } else if(strncmp(ln,".global",7)==0){
            char *v=trim(ln+7); char *end=NULL; long long gv=strtoll(v,&end,10);
            if(end==v){ rc=J_ERR_FMT; goto done; }
            if(nglobals==gcap){ uint32_t nc=gcap?gcap*2:8; int64_t*t=realloc(globals,nc*sizeof(int64_t)); if(!t){rc=J_ERR_OOM;goto done;} globals=t;gcap=nc; }
            globals[nglobals++]=gv;
            if(nglobals>J_MAX_GLOBALS){ rc=J_ERR_LIMIT; goto done; }
        } else if(strncmp(ln,".memory",7)==0){
            char *v=trim(ln+7); unsigned long pg=strtoul(v,NULL,10);
            if(pg>J_MAX_MEM_PAGES){ rc=J_ERR_LIMIT; goto done; }
            mem_pages=(uint32_t)pg;
        } else if(strncmp(ln,".function",9)==0){
            char np[32]={0},nl[32]={0},nm[J_MAX_NAME]={0};
            sscanf(ln+9,"%31s %31s %127s",np,nl,nm);
            if(nfuncs==fcap){ uint32_t nf=fcap?fcap*2:4; AFunc*t=realloc(funcs,nf*sizeof(AFunc)); if(!t){rc=J_ERR_OOM;goto done;} funcs=t;fcap=nf; }
            cur=&funcs[nfuncs++]; memset(cur,0,sizeof(*cur));
            cur->nparams=(uint32_t)strtoul(np,NULL,10); cur->nlocals=(uint32_t)strtoul(nl,NULL,10);
            scpy(cur->export_name,sizeof(cur->export_name),nm);
        } else if(strncmp(ln,".end",4)==0){
            cur=NULL;
        } else if(cur){
            size_t L=strlen(ln);
            if(ln[L-1]==':'){
                ln[L-1]='\0';
                char (*nl2)[64]=realloc(cur->labels,(cur->nlabels+1)*sizeof(*cur->labels));
                if(!nl2){ rc=J_ERR_OOM; goto done; }
                cur->labels=nl2;
                uint32_t *ni2=realloc(cur->label_idx,(cur->nlabels+1)*sizeof(uint32_t));
                if(!ni2){ rc=J_ERR_OOM; goto done; }
                cur->label_idx=ni2;
                scpy(cur->labels[cur->nlabels],64,ln);
                cur->label_idx[cur->nlabels]=cur->n;
                cur->nlabels++;
                continue;
            }
            char mn[32]={0}, opnd[64]={0};
            sscanf(ln,"%31s %63s",mn,opnd);
            int ho; int opc=mnemonic(mn,&ho);
            if(opc<0){ rc=J_ERR_FMT; goto done; }
            if(cur->n==cur->cap){ uint32_t nc=cur->cap?cur->cap*2:16; AIns*t=realloc(cur->ins,nc*sizeof(AIns)); if(!t){rc=J_ERR_OOM;goto done;} cur->ins=t;cur->cap=nc; }
            AIns *ai=&cur->ins[cur->n++]; memset(ai,0,sizeof(*ai)); ai->op=(uint8_t)opc;
            if(ho){
                if(!opnd[0]){ rc=J_ERR_FMT; goto done; }
                char *end=NULL; long long v=strtoll(opnd,&end,10);
                if(end!=opnd && *end=='\0'){ ai->operand=(long)v; }
                else { ai->is_label=1; scpy(ai->label,sizeof(ai->label),opnd); }
            }
        }
    }
    for(uint32_t f=0; f<nfuncs; f++){
        AFunc *fn=&funcs[f];
        for(uint32_t i=0;i<fn->n;i++) if(fn->ins[i].is_label){
            int found=0;
            for(uint32_t k=0;k<fn->nlabels;k++) if(strcmp(fn->labels[k],fn->ins[i].label)==0){ fn->ins[i].operand=(long)fn->label_idx[k]; fn->ins[i].is_label=0; found=1; break; }
            if(!found){ rc=J_ERR_FMT; goto done; }
        }
    }
    /* ---- 序列化（区段：id, size u32, payload）---- */
    {
        /* 先算 FUNCS / EXPORTS 负载大小 */
        size_t funcs_plen=4; for(uint32_t f=0;f<nfuncs;f++){ funcs_plen+=12; for(uint32_t i=0;i<funcs[f].n;i++) funcs_plen += (size_t)(1 + (has_operand(funcs[f].ins[i].op)?4:0)); }
        uint32_t ne=0; for(uint32_t f=0;f<nfuncs;f++) if(funcs[f].export_name[0] && strcmp(funcs[f].export_name,"-")!=0) ne++;
        size_t exp_plen=4; for(uint32_t f=0;f<nfuncs;f++) if(funcs[f].export_name[0] && strcmp(funcs[f].export_name,"-")!=0) exp_plen += 2+strlen(funcs[f].export_name)+4;
        size_t memsec = mem_pages ? (1+4+4) : 0;
        size_t globsec = nglobals ? (1+4+4+8*(size_t)nglobals) : 0;
        size_t total=4 + (1+4+4+nconsts*8) + (1+4+funcs_plen) + (1+4+exp_plen) + memsec + globsec;
        uint8_t *buf=malloc(total); if(!buf){rc=J_ERR_OOM;goto done;}
        uint8_t *p=buf;
        *p++='J';*p++='C';*p++='B';*p++=0x01;
        /* CONSTS */
        *p++=1; put_u32(&p,(uint32_t)(4+nconsts*8)); put_u32(&p,nconsts);
        for(uint32_t i=0;i<nconsts;i++) put_u64(&p,(uint64_t)consts[i]);
        /* FUNCS */
        *p++=2; put_u32(&p,(uint32_t)funcs_plen); put_u32(&p,nfuncs);
        for(uint32_t f=0;f<nfuncs;f++){
            put_u32(&p,funcs[f].nparams); put_u32(&p,funcs[f].nlocals);
            uint8_t *clen=p; p+=4; uint8_t *cstart=p;
            for(uint32_t i=0;i<funcs[f].n;i++){ *p++=funcs[f].ins[i].op; if(has_operand(funcs[f].ins[i].op)) put_u32(&p,(uint32_t)funcs[f].ins[i].operand); }
            uint32_t cl=(uint32_t)(p-cstart); memcpy(clen,&cl,4);
        }
        /* EXPORTS */
        *p++=7; put_u32(&p,(uint32_t)exp_plen); put_u32(&p,ne);
        for(uint32_t f=0;f<nfuncs;f++){
            if(!(funcs[f].export_name[0] && strcmp(funcs[f].export_name,"-")!=0)) continue;
            uint16_t nl=(uint16_t)strlen(funcs[f].export_name); put_u16(&p,nl);
            memcpy(p,funcs[f].export_name,nl); p+=nl; put_u32(&p,f);
        }
        if(mem_pages){ *p++=3; put_u32(&p,4); put_u32(&p,mem_pages); }
        if(nglobals){ *p++=4; put_u32(&p,(uint32_t)(4+8*nglobals)); put_u32(&p,nglobals); for(uint32_t i=0;i<nglobals;i++) put_u64(&p,(uint64_t)globals[i]); }
        *out_buf=buf; *out_len=(size_t)(p-buf);
    }
done:
    free(work); free(consts); free(globals);
    for(uint32_t f=0; f<nfuncs; f++){ free(funcs[f].ins); free(funcs[f].labels); free(funcs[f].label_idx); }
    free(funcs);
    if(rc!=J_OK){ *out_buf=NULL; *out_len=0; }
    return rc;
}
