/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include "jinvia/mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails=0;
static void ck(int c,const char*m){ if(!c){printf("  FAIL: %s\n",m);fails++;} }

/* 把一个多行请求集喂给服务器，返回堆上输出文本（NUL 结尾） */
static char *serve(const char *reqs){
    size_t rl=strlen(reqs); char *inb=malloc(rl+1); memcpy(inb,reqs,rl+1);
    FILE *in=fmemopen(inb,rl,"r");
    char *ob=NULL; size_t osz=0; FILE *out=open_memstream(&ob,&osz);
    if(!in||!out){ printf("  setup fail\n"); fails++; free(inb); return NULL; }
    j_mcp_serve("examples", in, out);
    fclose(in); fflush(out); fclose(out); free(inb);
    return ob; /* caller free */
}

int main(void){
    /* ---------- A. 协议正确性 ---------- */
    const char *A =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n"
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"sum-1-to-10\",\"arguments\":{}}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"evil\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"nope\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":\"x\",\"method\":\"ping\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"bogus/method\"}\n";
    char *a=serve(A);
    ck(a!=NULL,"A output");
    if(a){
        ck(strstr(a,"\"protocolVersion\":\"2025-06-18\"")!=NULL,"initialize protocolVersion");
        ck(strstr(a,"\"serverInfo\":{\"name\":\"jinvia\"")!=NULL,"serverInfo");
        ck(strstr(a,"thermostat-guard")!=NULL,"list tool thermostat-guard");
        ck(strstr(a,"\"inputSchema\"")!=NULL,"tools have inputSchema");
        ck(strstr(a,"result = 55")!=NULL,"call sum => 55");
        ck(strstr(a,"\"isError\":true")!=NULL,"evil isError");
        ck(strstr(a,"undeclared host capability")!=NULL,"evil default-deny");
        ck(strstr(a,"-32602")!=NULL,"unknown tool / missing name -> -32602");
        ck(strstr(a,"\"id\":\"x\"")!=NULL,"string id echoed");
        ck(strstr(a,"-32601")!=NULL,"unknown method -> -32601");
        free(a);
    }

    /* ---------- B. JSON 解析健壮性（不崩即可；顺带覆盖大量分支） ---------- */
    const char *B =
        "{\"jsonrpc\":\"2.0\",\"id\":\"a\\\"b\\\\c\\n\\t\\u00e9\",\"method\":\"ping\"}\n"        /* escapes */
        "{\"jsonrpc\":\"2.0\",\"id\":\"\\ud83d\\ude00\",\"method\":\"ping\"}\n"                     /* surrogate pair -> emoji */
        "{\"jsonrpc\":\"2.0\",\"id\":1.5e3,\"method\":\"ping\"}\n"                                  /* number exp */
        "{\"jsonrpc\":\"2.0\",\"id\":-42,\"method\":\"ping\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":12345678901234,\"method\":\"ping\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":true,\"method\":\"ping\"}\n"                                    /* bool id */
        "{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"ping\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/call\",\"params\":{\"name\":\"sum-1-to-10\",\"arguments\":{\"nested\":{\"a\":[1,2,3]}}}}\n"  /* nested ignored */
        "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"tools/list\",\"params\":{\"cursor\":\"abc\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"tools/list\"}\n"
        "{this is not json}\n"                                          /* -32700 */
        "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\n"                  /* truncated */
        "[1,2,3]\n"                                                     /* array not object -> -32600 */
        "123\n"                                                          /* number not object */
        "null\n"                                                         /* null not object */
        "\"just a string\"\n"
        "{}\n"                                                           /* empty obj -> no method */
        "{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"initialize\",\"params\":{\"clientInfo\":{\"name\":\"t\"}}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":14,\"method\":\"tools/call\",\"params\":{\"name\":\"door-guard\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":15,\"method\":\"tools/call\",\"params\":{\"name\":\"sensor-double\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":16,\"method\":\"tools/call\",\"params\":{\"name\":\"array-sum\"}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":17,\"method\":\"tools/call\",\"params\":{\"name\":\"thermostat-guard\"}}\n";
    char *b=serve(B);
    ck(b!=NULL,"B output");
    if(b){
        ck(strstr(b,"-32700")!=NULL,"malformed -> -32700 parse error");
        ck(strstr(b,"-32600")!=NULL,"non-object -> -32600 invalid request");
        ck(strstr(b,"result = 60")!=NULL,"array-sum => 60");
        ck(strstr(b,"result = 68")!=NULL,"sensor-double => 68");
        ck(strstr(b,"invalid request")!=NULL,"empty object -> invalid request");
        /* 代理对 -> 😀 UTF-8 F0 9F 98 80 应被正确解码后原样回显 */
        ck(strstr(b,"\xF0\x9F\x98\x80")!=NULL,"surrogate pair decoded to emoji");
        free(b);
    }

    /* ---------- C. 单行 API + 无工具目录/空输入边界 ---------- */
    char *ob=NULL; size_t osz=0; FILE *o=open_memstream(&ob,&osz);
    const char *pl="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}";
    ck(j_mcp_handle_line("examples",pl,strlen(pl),o)==0,"handle_line returns 0");
    fflush(o); fclose(o);
    ck(ob && strstr(ob,"\"result\":{}")!=NULL,"handle_line ping result");
    free(ob);
    /* serve with no trailing newline handled */
    char *c=serve("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}");  /* no newline */
    ck(c && strstr(c,"\"result\":{}")!=NULL,"no-trailing-newline handled");
    free(c);

    if(fails){ printf("test_mcp: %d FAIL\n",fails); return 1; }
    printf("test_mcp: ALL PASS\n");
    return 0;
}
