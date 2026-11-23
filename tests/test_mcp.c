/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include "jinvia/mcp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails=0;
static void ck(int c,const char*m){ if(!c){printf("  FAIL: %s\n",m);fails++;} }
static int count_lines(const char*s){ int n=0; for(; *s; s++) if(*s=='\n') n++; return n; }
static int contains(const char*h,const char*nd){ return strstr(h,nd)!=NULL; }

int main(void){
    const char *reqs =
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n"
        "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"sum-1-to-10\",\"arguments\":{}}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"evil\",\"arguments\":{}}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"nope\",\"arguments\":{}}}\n"
        "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"ping\"}\n"
        "{this is not json}\n";
    size_t rlen=strlen(reqs);
    char *inbuf=malloc(rlen+1); memcpy(inbuf,reqs,rlen+1);
    FILE *in=fmemopen(inbuf,rlen,"r"); ck(in!=NULL,"fmemopen");
    char *outbuf=NULL; size_t osz=0; FILE *out=open_memstream(&outbuf,&osz); ck(out!=NULL,"open_memstream");
    if(!in||!out){ printf("test_mcp: setup fail\n"); return 1; }

    j_mcp_serve("examples", in, out);
    fclose(in); fflush(out); fclose(out);

    /* 7 条请求有响应（notification 不响应，共 7 条：init/list/call/call/call/ping/parse-error），共 8 行请求里 1 条是 notification */
    ck(count_lines(outbuf)==7,"expected 7 responses");
    ck(contains(outbuf,"\"protocolVersion\":\"2025-06-18\""),"initialize protocolVersion");
    ck(contains(outbuf,"\"serverInfo\":{\"name\":\"jinvia\""),"serverInfo jinvia");
    ck(contains(outbuf,"\"tools\":["),"tools list present");
    ck(contains(outbuf,"thermostat-guard"),"tool thermostat-guard listed");
    ck(contains(outbuf,"sum-1-to-10"),"tool sum-1-to-10 listed");
    ck(contains(outbuf,"result = 55"),"tools/call sum => 55");
    /* evil: needs write:disk 未注册 -> 默认拒绝 -> isError true */
    ck(contains(outbuf,"\"isError\":true"),"evil call isError");
    ck(contains(outbuf,"undeclared host capability"),"evil refused (default deny)");
    ck(contains(outbuf,"-32602"),"unknown tool error code");
    ck(contains(outbuf,"-32700"),"parse error code");
    /* 确认工具都带 inputSchema */
    ck(contains(outbuf,"\"inputSchema\""),"tools have inputSchema");

    if(fails){ printf("test_mcp: %d FAIL\n",fails); free(outbuf); free(inbuf); return 1; }
    printf("test_mcp: ALL PASS\n");
    free(outbuf); free(inbuf); return 0;
}
