#include "jinvia/jinvia.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails=0;
static void ck(int c,const char*m,int ln){ if(!c){printf("  FAIL line %d: %s\n",ln,m);fails++;} }
static int64_t rd_temp(void*ud,int64_t a){(void)ud;(void)a;return 34;}
static int64_t alarm(void*ud,int64_t a){(void)ud;(void)a;return 0;}
static int64_t rd_motion(void*ud,int64_t a){(void)ud;(void)a;return 1;}
static int64_t open_door(void*ud,int64_t a){(void)ud;(void)a;return 0;}

/* 载入示例包并跑，返回结果/错误 */
static int load_run(const char*dir, int reg_alarm, int reg_extra, int64_t*out){
    char conf[8192],jp[1024]; static unsigned char jcb[1<<20]; size_t cl,jl;
    snprintf(jp,sizeof(jp),"%s/capability.conf",dir);
    FILE*f=fopen(jp,"rb"); if(!f) return -100; cl=fread(conf,1,sizeof(conf)-1,f); fclose(f); conf[cl]='\0';
    snprintf(jp,sizeof(jp),"%s/main.jcb",dir);
    f=fopen(jp,"rb"); if(!f) return -101; jl=fread(jcb,1,sizeof(jcb),f); fclose(f);

    JinviaRuntime*rt=jinvia_new();
    jinvia_register_host(rt,"read:temp",rd_temp,NULL);        /* 0 */
    if(reg_alarm) jinvia_register_host(rt,"call:alarm",alarm,NULL); /* 1 */
    jinvia_register_host(rt,"read:motion",rd_motion,NULL);    /* 2 */
    jinvia_register_host(rt,"open:door",open_door,NULL);      /* 3 */
    if(reg_extra) jinvia_register_host(rt,"write:disk",alarm,NULL); /* 4 */
    int rc=jinvia_load(rt,conf,jcb,jl);
    if(rc==JINVIA_OK && out){ rc=jinvia_call(rt,out); }
    jinvia_free(rt);
    return rc;
}

int main(void){
    int64_t r;
    /* thermostat（needs read:temp,call:alarm）都注册 -> 跑出 1 */
    ck(load_run("examples/thermostat-guard",1,0,&r)==J_OK && r==1,"thermostat via API = 1",__LINE__);
    /* thermostat 但没注册 call:alarm -> needs 拒绝 */
    ck(load_run("examples/thermostat-guard",0,0,&r)==JINVIA_ERR_NEEDS,"missing host -> NEEDS reject",__LINE__);
    /* door-guard（needs read:motion,open:door 注册 idx 2,3）-> 1 */
    ck(load_run("examples/door-guard",1,0,&r)==J_OK && r==1,"door-guard via API = 1",__LINE__);
    /* sum-1-to-10 无需宿主 -> 55 */
    ck(load_run("examples/sum-1-to-10",1,0,&r)==J_OK && r==55,"sum via API = 55",__LINE__);
    /* array-sum -> 60 */
    ck(load_run("examples/array-sum",1,0,&r)==J_OK && r==60,"array-sum via API = 60",__LINE__);
    /* evil：needs 含 write:disk，不注册 -> 拒绝 */
    ck(load_run("examples/evil",1,0,&r)==JINVIA_ERR_NEEDS,"evil(no write:disk) -> NEEDS reject",__LINE__);
    /* evil：注册了 write:disk -> 能加载（但字节码 HOST 处理…）*/
    { int rc=load_run("examples/evil",1,1,&r); ck(rc==J_OK || rc==JINVIA_ERR_HOSTIDX, "evil with write:disk loads+runs", __LINE__); }

    /* 基本 API 防御 */
    ck(jinvia_register_host(NULL,"x",rd_temp,NULL)==JINVIA_ERR_NULL,"register null rt",__LINE__);
    { JinviaRuntime*rt=jinvia_new(); ck(jinvia_call(rt,&r)==JINVIA_ERR_NOLOAD,"call before load",__LINE__); ck(jinvia_cap_name(rt)==NULL,"cap_name null",__LINE__); jinvia_free(rt); }
    { JinviaRuntime*t=jinvia_new(); ck(t!=NULL,"jinvia_new ok",__LINE__); jinvia_free(t); }

    /* 重复 load 替换、无泄漏（Valgrind 验证） */
    {
        JinviaRuntime*rt=jinvia_new();
        jinvia_register_host(rt,"read:temp",rd_temp,NULL);
        jinvia_register_host(rt,"call:alarm",alarm,NULL);
        char conf[8192]; static unsigned char jcb[1<<20]; size_t cl,jl; FILE*f;
        f=fopen("examples/thermostat-guard/capability.conf","rb"); cl=fread(conf,1,sizeof(conf)-1,f); fclose(f); conf[cl]='\0';
        f=fopen("examples/thermostat-guard/main.jcb","rb"); jl=fread(jcb,1,sizeof(jcb),f); fclose(f);
        ck(jinvia_load(rt,conf,jcb,jl)==JINVIA_OK,"load #1",__LINE__);
        ck(jinvia_load(rt,conf,jcb,jl)==JINVIA_OK,"load #2 (replace)",__LINE__);
        int64_t rr; ck(jinvia_call(rt,&rr)==J_OK && rr==1,"call after reload",__LINE__);
        jinvia_free(rt);
    }

    /* API 错误路径与查询（覆盖 jinvia.c 防御分支） */
    {
        JinviaRuntime *rt = jinvia_new();
        ck(jinvia_set_limits(rt, 1000000, 1024, 128) == JINVIA_OK, "set_limits ok", __LINE__);
        ck(jinvia_host_count(rt) == 0, "host_count 0", __LINE__);
        int i0 = jinvia_register_host(rt, "a", rd_temp, NULL);
        int i1 = jinvia_register_host(rt, "b", alarm, NULL);
        ck(i0 == 0 && i1 == 1 && jinvia_host_count(rt) == 2, "host idx sequential", __LINE__);
        char longn[128]; memset(longn, 'x', sizeof(longn)-1); longn[sizeof(longn)-1]='\0';
        ck(jinvia_register_host(rt, longn, rd_temp, NULL) == J_ERR_LIMIT, "register long name limit", __LINE__);
        ck(jinvia_register_host(rt, "c", NULL, NULL) == JINVIA_ERR_NULL, "register null fn", __LINE__);
        ck(jinvia_load(rt, NULL, NULL, 0) == JINVIA_ERR_NULL, "load null conf", __LINE__);
        ck(jinvia_load(rt, "name: x\n", (const uint8_t*)"BAD", 3) == J_ERR_FMT, "load bad jcb -> FMT", __LINE__);
        int64_t rr; ck(jinvia_call(rt, &rr) == JINVIA_ERR_NOLOAD, "call after failed load -> NOLOAD", __LINE__);
        jinvia_free(rt);
    }
    /* call_args：examples/add 带两个参数；形参个数/空参防御 */
    {
        char conf[8192]; static unsigned char jcb[1<<20]; size_t cl,jl; FILE*f;
        f=fopen("examples/add/capability.conf","rb"); cl=fread(conf,1,sizeof(conf)-1,f); fclose(f); conf[cl]='\0';
        f=fopen("examples/add/main.jcb","rb"); jl=fread(jcb,1,sizeof(jcb),f); fclose(f);
        JinviaRuntime*rt=jinvia_new();
        ck(jinvia_load(rt,conf,jcb,jl)==JINVIA_OK,"add load",__LINE__);
        ck(jinvia_entry_nparams(rt)==2,"add entry nparams 2",__LINE__);
        int64_t a[2]={7,5}; int64_t rr=0;
        ck(jinvia_call_args(rt,a,2,&rr)==J_OK && rr==12,"add call_args 7+5=12",__LINE__);
        int64_t wrong[1]={1};
        ck(jinvia_call_args(rt,wrong,1,&rr)==J_ERR_FMT,"add wrong nargs -> FMT",__LINE__);
        ck(jinvia_call_args(rt,NULL,2,&rr)==JINVIA_ERR_NULL,"null args -> NULL",__LINE__);
        jinvia_free(rt);
    }
    /* 线性内存 API + 字符串参数端到端（strlen 工具） */
    {
        char conf[8192]; static unsigned char jcb[1<<20]; size_t cl,jl; FILE*f;
        f=fopen("examples/strlen/capability.conf","rb"); cl=fread(conf,1,sizeof(conf)-1,f); fclose(f); conf[cl]='\0';
        f=fopen("examples/strlen/main.jcb","rb"); jl=fread(jcb,1,sizeof(jcb),f); fclose(f);
        JinviaRuntime*rt=jinvia_new();
        ck(jinvia_load(rt,conf,jcb,jl)==JINVIA_OK,"strlen load",__LINE__);
        uint32_t ms = jinvia_mem_size(rt);
        ck(ms == 4096, "strlen mem_size 4096", __LINE__);
        const char *str = "hello";
        ck(jinvia_mem_write(rt, 0, str, 5) == 0, "mem_write ok", __LINE__);
        { uint8_t nul=0; jinvia_mem_write(rt, 5, &nul, 1); }
        int64_t a[2]={0,5}; int64_t rr=0;
        ck(jinvia_call_args(rt,a,2,&rr)==J_OK && rr==5,"strlen via mem => 5",__LINE__);
        ck(jinvia_mem_write(rt, ms, str, 5) != 0, "mem_write OOB -> error", __LINE__);
        jinvia_free(rt);
    }
    if(fails==0){printf("ALL TESTS PASSED\n");return 0;}
    printf("%d TEST(S) FAILED\n",fails);return 1;
}
