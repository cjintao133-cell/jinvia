/* SPDX-License-Identifier: Apache-2.0 */
/* Jinvia CLI —— run 子命令直接基于库 jinvia.h（不再是独立写死的宿主表）。
 * asm / dis 子命令走 jcb_asm / jcb_disasm。 */
#include "jinvia/jinvia.h"
#include "jinvia/cap.h"
#include "jinvia/jcb.h"
#include "jinvia/cc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_trace = 0;

static const char *err_name(int rc) {
    switch (rc) {
        case JINVIA_OK: return "OK";
        case J_ERR_EOF: return "数据截断"; case J_ERR_MAGIC: return "坏魔数"; case J_ERR_VERSION: return "坏版本";
        case J_ERR_SECTION: return "区段越界"; case J_ERR_LIMIT: return "超出实现上限"; case J_ERR_OOM: return "内存不足";
        case J_ERR_FMT: return "格式/校验失败"; case J_ERR_VM: return "运行错误(越界/预算/除零)";
        case JINVIA_ERR_NEEDS: return "宿主未授权的本事(默认拒绝)"; case JINVIA_ERR_HOSTIDX: return "HOST 索引越界";
        case JINVIA_ERR_NOLOAD: return "未加载"; case JINVIA_ERR_NULL: return "空参数";
        default: return "未知错误";
    }
}

static int read_file(const char *path, unsigned char *buf, size_t cap, size_t *n) {
    FILE *f = fopen(path, "rb"); if (!f) return -1;
    *n = fread(buf, 1, cap, f); int err = ferror(f); fclose(f); return err ? -1 : 0;
}

/* 演示宿主：真实项目里这些就是你设备的读写接口 */
static int64_t h_read_temp(void *ud, int64_t arg) { (void)ud; if (g_trace) printf("    [host] read:temp(%lld) -> 34\n", (long long)arg); return 34; }
static int64_t h_alarm(void *ud, int64_t arg) { (void)ud; (void)arg; printf("    [host] call:alarm -> 🔔 报警!\n"); return 0; }
static int64_t h_read_motion(void *ud, int64_t arg) { (void)ud; if (g_trace) printf("    [host] read:motion(%lld) -> 1\n", (long long)arg); return 1; }
static int64_t h_open_door(void *ud, int64_t arg) { (void)ud; (void)arg; printf("    [host] open:door -> 🔓 开门\n"); return 0; }

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "asm") == 0) {
        if (argc < 4) { fprintf(stderr, "用法: jinvia asm <in.jcbasm> <out.jcb>\n"); return 2; }
        static char src[1 << 20]; size_t sl;
        if (read_file(argv[2], (unsigned char*)src, sizeof(src)-1, &sl) != 0) { fprintf(stderr, "读不到: %s\n", argv[2]); return 1; }
        src[sl] = '\0';
        uint8_t *out = NULL; size_t outlen = 0;
        int rc = j_asm(src, &out, &outlen);
        if (rc != J_OK) { fprintf(stderr, "汇编失败: %s\n", err_name(rc)); return 1; }
        FILE *f = fopen(argv[3], "wb"); if (!f) { free(out); fprintf(stderr, "写不了: %s\n", argv[3]); return 1; }
        fwrite(out, 1, outlen, f); fclose(f); free(out);
        printf("已汇编 %s -> %s (%zu 字节)\n", argv[2], argv[3], outlen); return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "dis") == 0) {
        if (argc < 3) { fprintf(stderr, "用法: jinvia dis <file.jcb>\n"); return 2; }
        static unsigned char jb[1 << 20]; size_t jl;
        if (read_file(argv[2], jb, sizeof(jb), &jl) != 0) { fprintf(stderr, "读不到: %s\n", argv[2]); return 1; }
        JModule m; int prc = j_parse(jb, jl, &m);
        if (prc != J_OK) { fprintf(stderr, "解析失败: %s\n", err_name(prc)); return 1; }
        static char out[1 << 20]; int n = j_disasm(&m, out, sizeof(out) - 1);
        if (n < 0) { fprintf(stderr, "反汇编失败: %s\n", err_name(n)); j_free(&m); return 1; }
        out[n] = '\0'; fputs(out, stdout); j_free(&m); return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "cc") == 0) {
        if (argc < 4) { fprintf(stderr, "用法: jinvia cc <in.c> <out.jcbasm>\n"); return 2; }
        static char csrc[1 << 20]; size_t clen;
        if (read_file(argv[2], (unsigned char*)csrc, sizeof(csrc)-1, &clen) != 0) { fprintf(stderr, "读不到: %s\n", argv[2]); return 1; }
        csrc[clen] = '\0';
        char *asmtxt = NULL, *cerr = NULL;
        int crc = j_cc_compile(csrc, &asmtxt, &cerr);
        if (crc != 0) { fprintf(stderr, "编译失败: %s\n", cerr ? cerr : "?"); free(cerr); return 1; }
        free(cerr);
        FILE *cf = fopen(argv[3], "wb"); if (!cf) { free(asmtxt); fprintf(stderr, "写不了: %s\n", argv[3]); return 1; }
        fwrite(asmtxt, 1, strlen(asmtxt), cf); fclose(cf); free(asmtxt);
        printf("已编译 C 子集 %s -> %s\n", argv[2], argv[3]); return 0;
    }
    if (argc < 3 || strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "用法:\n  jinvia run [--trace] <能力目录>\n  jinvia asm <in.jcbasm> <out.jcb>\n  jinvia dis <file.jcb>\n  jinvia cc <in.c> <out.jcbasm>\n");
        return 2;
    }
    int ai = 2; if (strcmp(argv[2], "--trace") == 0) { g_trace = 1; ai = 3; }
    if (ai >= argc) { fprintf(stderr, "缺少能力目录\n"); return 2; }
    const char *dir = argv[ai];

    /* 读能力声明 + 字节码 */
    char cpath[1024], jpath[1024]; static char ctext[8192]; static unsigned char jbuf[1 << 20]; size_t cl, jl;
    snprintf(cpath, sizeof(cpath), "%s/capability.conf", dir);
    if (read_file(cpath, (unsigned char*)ctext, sizeof(ctext)-1, &cl) != 0) { fprintf(stderr, "读不到: %s\n", cpath); return 1; }
    ctext[cl] = '\0';
    SrCap cap;
    if (sr_cap_parse(ctext, &cap) != 0) { fprintf(stderr, "配置解析失败\n"); return 1; }
    snprintf(jpath, sizeof(jpath), "%s/%s", dir, cap.entry[0] ? cap.entry : "main.jcb");
    if (read_file(jpath, jbuf, sizeof(jbuf), &jl) != 0) { fprintf(stderr, "读不到字节码: %s\n", jpath); return 1; }

    /* 用库：注册你能提供的宿主功能；能力申请了没注册的 -> 默认拒绝 */
    JinviaRuntime *rt = jinvia_new();
    jinvia_register_host(rt, "read:temp", h_read_temp, NULL);
    jinvia_register_host(rt, "call:alarm", h_alarm, NULL);
    jinvia_register_host(rt, "read:motion", h_read_motion, NULL);
    jinvia_register_host(rt, "open:door", h_open_door, NULL);
    jinvia_set_limits(rt, 10000000ULL, 4096, 256);

    int rc = jinvia_load(rt, ctext, jbuf, jl);
    if (rc != JINVIA_OK) {
        printf("载入能力: %s —— %s\n", cap.name, cap.does);
        printf("拒绝运行: %s\n", err_name(rc));
        jinvia_free(rt); return (rc == JINVIA_ERR_NEEDS || rc == JINVIA_ERR_HOSTIDX) ? 3 : 1;
    }
    printf("载入能力: %s —— %s\n", jinvia_cap_name(rt), cap.does);
    printf("  申请本事(%d):", cap.needs_count);
    for (int i = 0; i < cap.needs_count; i++) printf(" %s", cap.needs[i]);
    printf("\n授权通过，执行 %s ...\n", jinvia_cap_name(rt));

    int64_t result = 0;
    rc = jinvia_call(rt, &result);
    jinvia_free(rt);
    if (rc != J_OK) { printf("执行失败: %s\n", err_name(rc)); return 4; }
    printf("结果 = %lld\n", (long long)result);
    return 0;
}
