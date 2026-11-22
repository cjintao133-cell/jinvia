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

    if (fails == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", fails);
    return 1;
}
