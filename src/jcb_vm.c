/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jcb.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define J_VM_STACK_CAP   4096     /* 栈容量上限（物理） */
#define J_VM_FRAMES_CAP  256      /* 帧容量上限（物理） */
#define J_VM_STEPS_CAP   100000000ULL

struct JVM {
    const JModule *m;
    JInstr  **ins;   uint32_t *nins;   int err;
    int64_t  *stack; uint32_t sp;
    JHostFn   host;  void *host_ud;
    JLimits   lim;
    uint64_t  max_millis;   /* 墙钟时间预算(ms),0=不限; 独立于 JLimits 以免 ABI 破坏 */
    uint8_t  *mem; uint32_t mem_size;
    int64_t  *globals; uint32_t nglobals;
};

JLimits j_vm_default_limits(void){
    JLimits l; l.max_steps = 10000000ULL; l.max_stack = 4096; l.max_frames = 256; return l;
}
void j_vm_set_time_budget(JVM *vm, uint64_t ms){ if(!vm) return; vm->max_millis = ms; }
void j_vm_set_limits(JVM *vm, JLimits lim){
    if(!vm) return;
    JLimits d = j_vm_default_limits();
    if(lim.max_steps  == 0) lim.max_steps  = d.max_steps;      /* 0=默认 */
    else if(lim.max_steps > J_VM_STEPS_CAP) lim.max_steps = J_VM_STEPS_CAP;
    if(lim.max_stack  == 0 || lim.max_stack  > J_VM_STACK_CAP)  lim.max_stack  = J_VM_STACK_CAP;
    if(lim.max_frames == 0 || lim.max_frames > J_VM_FRAMES_CAP) lim.max_frames = J_VM_FRAMES_CAP;
    vm->lim = lim;
}
typedef struct { uint32_t func, pc; int64_t *locals; uint32_t nlocals; } Frame;

static int pushv(JVM *vm, int64_t v){ if(vm->sp>=vm->lim.max_stack) return J_ERR_VM; vm->stack[vm->sp++]=v; return J_OK; }
static int popv(JVM *vm, int64_t *v){ if(vm->sp<1) return J_ERR_VM; *v=vm->stack[--vm->sp]; return J_OK; }
static double bits_to_d(int64_t v){ double d; memcpy(&d,&v,8); return d; }
static int64_t d_to_bits(double d){ int64_t v; memcpy(&v,&d,8); return v; }
static float low_to_f(int64_t v){ uint32_t u=(uint32_t)v; float f; memcpy(&f,&u,4); return f; }
static int64_t f_to_low(float f){ uint32_t u; memcpy(&u,&f,4); return (int64_t)u; }
static int64_t sat_double_to_i64(double d){
    if(d!=d) return 0;                         /* NaN -> 0 */
    if(d>=9223372036854775808.0) return INT64_MAX;
    if(d< -9223372036854775808.0) return INT64_MIN;
    return (int64_t)d;
}
static int64_t sat_float_to_i64(float f){
    if(f!=f) return 0;
    if(f>=9223372036854775808.0f) return INT64_MAX;
    if(f< -9223372036854775808.0f) return INT64_MIN;
    return (int64_t)f;
}

JVM *j_vm_new(const JModule *m){
    if(!m) return NULL;
    JVM *vm=(JVM*)calloc(1,sizeof(JVM)); if(!vm) return NULL;
    vm->m=m;
    uint32_t nf=m->nfuncs?m->nfuncs:1;
    vm->ins=(JInstr**)calloc(nf,sizeof(JInstr*));
    vm->nins=(uint32_t*)calloc(nf,sizeof(uint32_t));
    vm->stack=(int64_t*)malloc(J_VM_STACK_CAP*sizeof(int64_t));
    if(!vm->ins||!vm->nins||!vm->stack){ j_vm_free(vm); return NULL; }
    if(m->mem_pages){
        vm->mem_size = (uint32_t)m->mem_pages * J_PAGE;
        vm->mem = (uint8_t*)calloc(vm->mem_size, 1);
        if(!vm->mem){ j_vm_free(vm); return NULL; }
    }
    if(m->nglobals){
        vm->nglobals = m->nglobals;
        vm->globals = (int64_t*)malloc((size_t)vm->nglobals*sizeof(int64_t));
        if(!vm->globals){ j_vm_free(vm); return NULL; }
        for(uint32_t i=0;i<vm->nglobals;i++) vm->globals[i]=m->globals[i];
    }
    for(uint32_t i=0;i<m->nfuncs;i++){
        JInstr *ins=NULL; uint32_t n=0;
        if(j_decode(m->funcs[i].code,m->funcs[i].code_len,&ins,&n)!=J_OK){ vm->err=J_ERR_FMT; continue; }
        if(j_validate_func(m,i,ins,n)!=J_OK){ free(ins); vm->err=J_ERR_FMT; continue; }
        vm->ins[i]=ins; vm->nins[i]=n;
    }
    vm->lim = j_vm_default_limits();
    vm->max_millis = 0;
    return vm;
}
void j_vm_free(JVM *vm){
    if(!vm) return;
    if(vm->ins){ for(uint32_t i=0;i<vm->m->nfuncs;i++) free(vm->ins[i]); free(vm->ins); }
    free(vm->nins); free(vm->stack); free(vm->mem); free(vm->globals); free(vm);
}
void j_vm_set_host(JVM *vm, JHostFn fn, void *ud){ if(vm){ vm->host=fn; vm->host_ud=ud; } }

static int enter(JVM *vm, uint32_t f, Frame *frames, uint32_t *nf){
    if(*nf>=vm->lim.max_frames) return J_ERR_VM;
    uint32_t np=vm->m->funcs[f].nparams, nl=vm->m->funcs[f].nlocals, tot=np+nl;
    int64_t *loc=NULL;
    if(tot){ loc=(int64_t*)calloc(tot,sizeof(int64_t)); if(!loc) return J_ERR_OOM; }
    if(vm->sp<np){ free(loc); return J_ERR_VM; }
    if(np>0 && !loc){ return J_ERR_OOM; } /* 防御：np>0 时 tot>=np>0 必已分配，此分支不应触发 */
    for(uint32_t i=0;i<np;i++) loc[np-1-i]=vm->stack[--vm->sp];
    frames[*nf].func=f; frames[*nf].pc=0; frames[*nf].locals=loc; frames[*nf].nlocals=tot; (*nf)++;
    return J_OK;
}

uint32_t j_vm_mem_size(JVM *vm){ return vm ? vm->mem_size : 0; }
int j_vm_mem_write(JVM *vm, uint32_t off, const void *src, uint32_t len){
    if(!vm) return -1;
    if(len && !src) return -1;
    if((uint64_t)off + (uint64_t)len > (uint64_t)vm->mem_size) return -1;
    if(len && vm->mem) memcpy(vm->mem + off, src, len);
    return 0;
}

int j_vm_call(JVM *vm, uint32_t func, const int64_t *args, uint32_t nargs, int64_t *result){
    if(!vm || vm->err) return J_ERR_FMT;
    if(func>=vm->m->nfuncs) return J_ERR_FMT;
    if(nargs!=vm->m->funcs[func].nparams) return J_ERR_FMT;
    vm->sp=0;
    for(uint32_t i=0;i<nargs;i++){ int r=pushv(vm,args[i]); if(r) return r; }

    Frame frames[J_VM_FRAMES_CAP]; uint32_t nf=0;
    int rc=J_OK; int64_t res=0; uint64_t steps=0; clock_t t0=clock();
    if((rc=enter(vm,func,frames,&nf))) goto cleanup;

    while(nf>0){
        if(++steps>vm->lim.max_steps){ rc=J_ERR_VM; break; }
        if(vm->max_millis && (steps & 0xFFFu)==0u){ if(((clock()-t0)*1000.0/(double)CLOCKS_PER_SEC) > (double)vm->max_millis){ rc=J_ERR_VM; break; } }
        Frame *fr=&frames[nf-1];
        if(fr->pc>=vm->nins[fr->func]){ rc=J_ERR_VM; break; }
        JInstr in=vm->ins[fr->func][fr->pc];
        switch(in.op){
            case OP_NOP: fr->pc++; break;
            case OP_PUSH_C: if(pushv(vm,vm->m->consts[in.arg])){rc=J_ERR_VM;break;} fr->pc++; break;
            case OP_DUP:{ int64_t t; if(popv(vm,&t)||pushv(vm,t)||pushv(vm,t)){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_DROP:{ int64_t t; if(popv(vm,&t)){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_LGET: if(in.arg>=fr->nlocals||pushv(vm,fr->locals[in.arg])){rc=J_ERR_VM;break;} fr->pc++; break;
            case OP_LSET:{ int64_t v; if(in.arg>=fr->nlocals||popv(vm,&v)){rc=J_ERR_VM;break;} fr->locals[in.arg]=v; fr->pc++; }break;
            case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                int64_t r=0;
                /* 用无符号运算实现定义良好的二补回绕，避免有符号溢出 UB */
                if(in.op==OP_ADD) r=(int64_t)((uint64_t)x + (uint64_t)y);
                else if(in.op==OP_SUB) r=(int64_t)((uint64_t)x - (uint64_t)y);
                else if(in.op==OP_MUL) r=(int64_t)((uint64_t)x * (uint64_t)y);
                else if(in.op==OP_DIV){
                    if(y==0 || (x==INT64_MIN && y==-1)){rc=J_ERR_VM;break;}  /* 防除零 + 防 SIGFPE */
                    r=x/y;
                } else {
                    if(y==0){rc=J_ERR_VM;break;}
                    r=(x==INT64_MIN && y==-1) ? 0 : x%y;                   /* INT64_MIN%-1 数学为 0 */
                }
                if(pushv(vm,r)){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_EQ: case OP_LT: case OP_GT: case OP_LE: case OP_GE:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                int64_t r=0;
                if(in.op==OP_EQ)r=(x==y); else if(in.op==OP_LT)r=(x<y); else if(in.op==OP_GT)r=(x>y);
                else if(in.op==OP_LE)r=(x<=y); else r=(x>=y);
                if(pushv(vm,r)){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_AND: case OP_OR: case OP_XOR:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                int64_t r = (in.op==OP_AND)?(x&y):(in.op==OP_OR)?(x|y):(x^y);
                if(pushv(vm,r)){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_SHL: case OP_SHR:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                unsigned sh=(unsigned)((uint64_t)y & 63u);
                int64_t r = (in.op==OP_SHL) ? (int64_t)((uint64_t)x << sh) : (int64_t)((uint64_t)x >> sh);
                if(pushv(vm,r)){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_NOT:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,~x)){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_SWAP:{ if(vm->sp<2){rc=J_ERR_VM;break;} int64_t t=vm->stack[vm->sp-1]; vm->stack[vm->sp-1]=vm->stack[vm->sp-2]; vm->stack[vm->sp-2]=t; fr->pc++; }break;
            case OP_OVER:{ if(vm->sp<2){rc=J_ERR_VM;break;} if(pushv(vm,vm->stack[vm->sp-2])){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_JMP: fr->pc=in.arg; break;
            case OP_JZ:{ int64_t c; if(popv(vm,&c)){rc=J_ERR_VM;break;} fr->pc=(c==0)?in.arg:fr->pc+1; }break;
            case OP_JNZ:{ int64_t c; if(popv(vm,&c)){rc=J_ERR_VM;break;} fr->pc=(c!=0)?in.arg:fr->pc+1; }break;
            case OP_CALL: fr->pc++; if((rc=enter(vm,in.arg,frames,&nf))) goto cleanup; break;
            case OP_HOST:{
                if(!vm->host){rc=J_ERR_VM;break;}
                int64_t a; if(popv(vm,&a)){rc=J_ERR_VM;break;}
                int64_t r=vm->host(vm->host_ud,in.arg,a);
                if(pushv(vm,r)){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_RET:{
                int64_t rv; if(popv(vm,&rv)){rc=J_ERR_VM;break;}
                free(fr->locals); nf--;
                if(nf==0){ res=rv; rc=J_OK; goto cleanup; }
                if(pushv(vm,rv)){rc=J_ERR_VM;break;}
            }break;
            case OP_MEM_LOAD32:{ int64_t a; if(popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0){rc=J_ERR_VM;break;} uint64_t ua=(uint64_t)a;
                if((uint64_t)vm->mem_size<4u || ua>(uint64_t)vm->mem_size-4u){rc=J_ERR_VM;break;}
                uint64_t v=0; for(int i=0;i<4;i++) v|=(uint64_t)vm->mem[ua+(uint64_t)i]<<(8u*(unsigned)i);
                if(pushv(vm,(int64_t)v)){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_MEM_STORE32:{ int64_t v,a; if(popv(vm,&v)||popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0){rc=J_ERR_VM;break;} uint64_t ua=(uint64_t)a;
                if((uint64_t)vm->mem_size<4u || ua>(uint64_t)vm->mem_size-4u){rc=J_ERR_VM;break;}
                uint64_t uv=(uint64_t)v; for(int i=0;i<4;i++) vm->mem[ua+(uint64_t)i]=(uint8_t)(uv>>(8u*(unsigned)i));
                fr->pc++; }break;
            case OP_MEM_FILL:{ int64_t bv,ln,a; if(popv(vm,&bv)||popv(vm,&ln)||popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0||ln<0){rc=J_ERR_VM;break;}
                uint64_t ua=(uint64_t)a, ul=(uint64_t)ln;
                if(ul>(uint64_t)vm->mem_size || ua>(uint64_t)vm->mem_size-ul){rc=J_ERR_VM;break;}
                if(ul==0){fr->pc++;break;}
                memset(vm->mem+ua,(int)(bv&0xff),(size_t)ul); fr->pc++; }break;
            case OP_MEM_COPY:{ int64_t ln,src,dst; if(popv(vm,&ln)||popv(vm,&src)||popv(vm,&dst)){rc=J_ERR_VM;break;}
                if(src<0||dst<0||ln<0){rc=J_ERR_VM;break;}
                uint64_t us=(uint64_t)src, ud=(uint64_t)dst, ul=(uint64_t)ln, ms=(uint64_t)vm->mem_size;
                if(ul>ms || us>ms-ul || ud>ms-ul){rc=J_ERR_VM;break;}
                if(ul==0){fr->pc++;break;}
                memmove(vm->mem+ud, vm->mem+us, (size_t)ul); fr->pc++; }break;
            case OP_G_GET:{ if(in.arg>=vm->nglobals){rc=J_ERR_VM;break;} if(pushv(vm,vm->globals[in.arg])){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_G_SET:{ int64_t v; if(popv(vm,&v)){rc=J_ERR_VM;break;} if(in.arg>=vm->nglobals){rc=J_ERR_VM;break;} vm->globals[in.arg]=v; fr->pc++; }break;
            case OP_F64_ADD: case OP_F64_SUB: case OP_F64_MUL: case OP_F64_DIV:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                double a=bits_to_d(x),b=bits_to_d(y),r;
                if(in.op==OP_F64_ADD)r=a+b; else if(in.op==OP_F64_SUB)r=a-b; else if(in.op==OP_F64_MUL)r=a*b; else r=a/b;
                if(pushv(vm,d_to_bits(r))){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_F64_NEG:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,d_to_bits(-bits_to_d(x)))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_F64_ABS:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} double d=bits_to_d(x); if(d<0)d=-d; if(pushv(vm,d_to_bits(d))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_F64_EQ: case OP_F64_LT: case OP_F64_GT: case OP_F64_LE: case OP_F64_GE:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                double a=bits_to_d(x),b=bits_to_d(y); int64_t r=0;
                if(in.op==OP_F64_EQ)r=(a==b); else if(in.op==OP_F64_LT)r=(a<b); else if(in.op==OP_F64_GT)r=(a>b); else if(in.op==OP_F64_LE)r=(a<=b); else r=(a>=b);
                if(pushv(vm,r)){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_I64_TO_F64:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,d_to_bits((double)x))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_F64_TO_I64:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,sat_double_to_i64(bits_to_d(x)))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_F32_ADD: case OP_F32_SUB: case OP_F32_MUL: case OP_F32_DIV:{
                int64_t y,x; if(popv(vm,&y)||popv(vm,&x)){rc=J_ERR_VM;break;}
                float a=low_to_f(x),b=low_to_f(y),r;
                if(in.op==OP_F32_ADD)r=a+b; else if(in.op==OP_F32_SUB)r=a-b; else if(in.op==OP_F32_MUL)r=a*b; else r=a/b;
                if(pushv(vm,f_to_low(r))){rc=J_ERR_VM;break;} fr->pc++;
            }break;
            case OP_F32_TO_F64:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,d_to_bits((double)low_to_f(x)))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_F64_TO_F32:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,f_to_low((float)bits_to_d(x)))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_I64_TO_F32:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,f_to_low((float)x))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_F32_TO_I64:{ int64_t x; if(popv(vm,&x)){rc=J_ERR_VM;break;} if(pushv(vm,sat_float_to_i64(low_to_f(x)))){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_MEM_SIZE: if(pushv(vm,(int64_t)vm->mem_size)){rc=J_ERR_VM;break;} fr->pc++; break;
            case OP_MEM_LOAD:{ int64_t a; if(popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0){rc=J_ERR_VM;break;} uint64_t ua=(uint64_t)a;
                if((uint64_t)vm->mem_size < 8u || ua > (uint64_t)vm->mem_size - 8u){rc=J_ERR_VM;break;}
                uint64_t v=0; for(int i=0;i<8;i++) v|=(uint64_t)vm->mem[ua+(uint64_t)i]<<(8u*(unsigned)i);
                if(pushv(vm,(int64_t)v)){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_MEM_STORE:{ int64_t v,a; if(popv(vm,&v)||popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0){rc=J_ERR_VM;break;} uint64_t ua=(uint64_t)a;
                if((uint64_t)vm->mem_size < 8u || ua > (uint64_t)vm->mem_size - 8u){rc=J_ERR_VM;break;}
                uint64_t uv=(uint64_t)v; for(int i=0;i<8;i++) vm->mem[ua+(uint64_t)i]=(uint8_t)(uv>>(8u*(unsigned)i));
                fr->pc++; }break;
            case OP_MEM_LOAD8:{ int64_t a; if(popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0){rc=J_ERR_VM;break;} uint64_t ua=(uint64_t)a; if(ua>=(uint64_t)vm->mem_size){rc=J_ERR_VM;break;}
                if(pushv(vm,(int64_t)vm->mem[ua])){rc=J_ERR_VM;break;} fr->pc++; }break;
            case OP_MEM_STORE8:{ int64_t v,a; if(popv(vm,&v)||popv(vm,&a)){rc=J_ERR_VM;break;}
                if(a<0){rc=J_ERR_VM;break;} uint64_t ua=(uint64_t)a; if(ua>=(uint64_t)vm->mem_size){rc=J_ERR_VM;break;}
                vm->mem[ua]=(uint8_t)v; fr->pc++; }break;
            case OP_HALT:{ res=(vm->sp>0)?vm->stack[vm->sp-1]:0; rc=J_OK; goto cleanup; }
            default: rc=J_ERR_VM; break;
        }
        if(rc) break;
    }
cleanup:
    for(uint32_t i=0;i<nf;i++) free(frames[i].locals);
    if(rc==J_OK && result) *result=res;
    return rc;
}
