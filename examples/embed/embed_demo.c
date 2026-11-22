/* 嵌入示例：把 Jinvia 当库用，宿主自己注册功能、加载能力、调用。
 * 编译：gcc -Iinclude examples/embed/embed_demo.c build/libjinvia.a -lm -o embed_demo
 */
#include "jinvia/jinvia.h"
#include <stdio.h>

/* 宿主的真实功能（这里用假数据演示） */
static int64_t my_read_temp(void *ud, int64_t arg) { (void)ud; (void)arg; return 34; }
static int64_t my_alarm(void *ud, int64_t arg) { (void)ud; (void)arg; printf("    🔔 宿主：报警已触发！\n"); return 0; }

static int read_file(const char *p, char *buf, size_t cap, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return -1;
    *n = fread(buf, 1, cap, f); fclose(f); return 0;
}

int main(int argc, char **argv) {
    const char *dir = (argc > 1) ? argv[1] : "examples/thermostat-guard";

    JinviaRuntime *rt = jinvia_new();
    /* 宿主注册自己的能力——顺序决定能力里的 HOST 索引。没注册的，能力要了也白搭。 */
    jinvia_register_host(rt, "read:temp", my_read_temp, NULL);   /* idx 0 */
    jinvia_register_host(rt, "call:alarm", my_alarm, NULL);      /* idx 1 */
    jinvia_set_limits(rt, 1000000, 4096, 256);

    char conf[8192], jpath[1024]; static unsigned char jcb[1 << 20]; size_t cl, jl;
    snprintf(jpath, sizeof(jpath), "%s/capability.conf", dir);
    if (read_file(jpath, conf, sizeof(conf) - 1, &cl) != 0) { fprintf(stderr, "读不到 conf\n"); jinvia_free(rt); return 1; }
    conf[cl] = '\0';
    snprintf(jpath, sizeof(jpath), "%s/main.jcb", dir);
    if (read_file(jpath, (char*)jcb, sizeof(jcb), &jl) != 0) { fprintf(stderr, "读不到 jcb\n"); jinvia_free(rt); return 1; }

    int rc = jinvia_load(rt, conf, jcb, jl);
    if (rc != JINVIA_OK) {
        printf("加载失败: rc=%d %s\n", rc,
               rc == JINVIA_ERR_NEEDS ? "(申请的 needs 宿主没注册 → 默认拒绝)" : "");
        jinvia_free(rt); return 1;
    }
    printf("已加载能力: %s\n", jinvia_cap_name(rt));
    int64_t res = 0;
    rc = jinvia_call(rt, &res);
    if (rc != J_OK) { printf("执行失败: rc=%d\n", rc); jinvia_free(rt); return 1; }
    printf("宿主收到结果 = %lld\n", (long long)res);

    jinvia_free(rt);
    return 0;
}
