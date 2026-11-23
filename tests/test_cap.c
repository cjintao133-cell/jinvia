#include "jinvia/cap.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
static void ck(int cond, const char *msg, int line) {
    if (!cond) { printf("  FAIL line %d: %s\n", line, msg); fails++; }
}

int main(void) {
    SrCap c;

    /* 1) 基本解析（含注释、逗号分隔 needs） */
    const char *t1 =
        "# comment line\n"
        "name: thermostat-guard\n"
        "does: read temp, alarm when over 30\n"
        "needs: read:temp, call:alarm\n"
        "entry: main.wasm\n";
    ck(sr_cap_parse(t1, &c) == 0, "parse returns 0", __LINE__);
    ck(strcmp(c.name, "thermostat-guard") == 0, "name", __LINE__);
    ck(strcmp(c.does, "read temp, alarm when over 30") == 0, "does", __LINE__);
    ck(strcmp(c.entry, "main.wasm") == 0, "entry", __LINE__);
    ck(c.needs_count == 2, "needs count == 2", __LINE__);
    ck(strcmp(c.needs[0], "read:temp") == 0, "need0", __LINE__);
    ck(strcmp(c.needs[1], "call:alarm") == 0, "need1", __LINE__);

    /* 2) NULL 防御 */
    ck(sr_cap_parse(NULL, &c) == -1, "null text -> -1", __LINE__);
    ck(sr_cap_parse(t1, NULL) == -1, "null out -> -1", __LINE__);

    /* 3) 缺 name -> -1 */
    ck(sr_cap_parse("does: x\nentry: y\n", &c) == -1, "missing name -> -1", __LINE__);

    /* 4) 只有空行/注释 -> -1 */
    ck(sr_cap_parse("\n# hi\n   \n", &c) == -1, "no name -> -1", __LINE__);

    /* 5) 单个 need（无逗号） */
    ck(sr_cap_parse("name: a\nneeds: only\n", &c) == 0 &&
       c.needs_count == 1 && strcmp(c.needs[0], "only") == 0, "single need", __LINE__);

    /* 6) 值里含冒号（entry 是 URL 之类）不被误切 */
    ck(sr_cap_parse("name: b\nentry: http://x/y\n", &c) == 0 &&
       strcmp(c.entry, "http://x/y") == 0, "colon in value", __LINE__);

    /* 7) needs_satisfied：全授权 -> 1 */
    {
        SrCap g;
        sr_cap_parse("name: g\nneeds: read:temp, call:alarm\n", &g);
        const char *gr1[] = {"read:temp", "call:alarm", "write:disk"};
        ck(sr_cap_needs_satisfied(&g, gr1, 3) == 1, "all needs granted -> 1", __LINE__);
        const char *gr2[] = {"read:temp"};
        ck(sr_cap_needs_satisfied(&g, gr2, 1) == 0, "missing one -> 0 (deny)", __LINE__);
        const char *gr3[] = {"write:disk"};
        ck(sr_cap_needs_satisfied(&g, gr3, 1) == 0, "none granted -> 0", __LINE__);
    }
    /* 8) 无 needs -> 恒放行 */
    {
        SrCap n;
        sr_cap_parse("name: n\n", &n);
        const char *gr[] = {"x"};
        ck(sr_cap_needs_satisfied(&n, gr, 1) == 1, "no needs -> 1", __LINE__);
    }

    /* 9) params 解析：name 与 name:type */
    {
        SrCap p;
        ck(sr_cap_parse("name: add\nparams: a, b, flag:bool\n", &p) == 0, "parse params ok", __LINE__);
        ck(p.params_count == 3, "params count 3", __LINE__);
        ck(strcmp(p.params[0].name, "a") == 0 && strcmp(p.params[0].type, "int") == 0, "param a int", __LINE__);
        ck(strcmp(p.params[1].name, "b") == 0 && strcmp(p.params[1].type, "int") == 0, "param b int", __LINE__);
        ck(strcmp(p.params[2].name, "flag") == 0 && strcmp(p.params[2].type, "bool") == 0, "param flag bool", __LINE__);
    }
    /* 10) 无 params -> count 0 */
    {
        SrCap q;
        ck(sr_cap_parse("name: x\n", &q) == 0 && q.params_count == 0, "no params -> 0", __LINE__);
    }
    /* 11) params 里的空格/冒号间距容错 */
    {
        SrCap w;
        ck(sr_cap_parse("name: y\nparams:  a : int ,  b\n", &w) == 0 &&
           w.params_count == 2 && strcmp(w.params[0].name,"a")==0 && strcmp(w.params[0].type,"int")==0 &&
           strcmp(w.params[1].name,"b")==0, "params whitespace/colon tolerant", __LINE__);
    }

    /* 12) params 字符串类型 */
    {
        SrCap sv;
        ck(sr_cap_parse("name: strlen\nparams: s:string, n:int\n", &sv) == 0, "parse string param", __LINE__);
        ck(sv.params_count == 2, "string params count 2", __LINE__);
        ck(strcmp(sv.params[0].name,"s")==0 && strcmp(sv.params[0].type,"string")==0, "param s string", __LINE__);
        ck(strcmp(sv.params[1].name,"n")==0 && strcmp(sv.params[1].type,"int")==0, "param n int", __LINE__);
    }

    if (fails == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", fails);
    return 1;
}
