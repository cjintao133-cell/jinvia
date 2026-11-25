/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jinvia.h"
#include "jinvia/cap.h"
#include <stdlib.h>
#include <string.h>

#define JINVIA_MAX_HOST 64

typedef struct { char name[64]; JinviaHostFn fn; void *ud; } JHost;
struct JinviaRuntime {
    JHost hosts[JINVIA_MAX_HOST];
    int   nhosts;
    JLimits lim;
    uint64_t lim_millis;   /* 墙钟时间预算(ms) */
    /* 当前已加载能力 */
    SrCap   cap;
    JModule mod;
    JVM    *vm;
    uint32_t entry;
    int     loaded;
    /* 能力 needs -> 宿主 idx 绑定（按 needs 顺序不必要；HOST idx 直接用宿主注册顺序） */
};

static int64_t host_bridge(void *ud, uint32_t idx, int64_t arg) {
    JinviaRuntime *rt = (JinviaRuntime *)ud;
    if (!rt || idx >= (uint32_t)rt->nhosts) return 0;
    return rt->hosts[idx].fn ? rt->hosts[idx].fn(rt->hosts[idx].ud, arg) : 0;
}

JinviaRuntime *jinvia_new(void) {
    JinviaRuntime *rt = (JinviaRuntime *)calloc(1, sizeof(JinviaRuntime));
    if (!rt) return NULL;
    rt->lim = j_vm_default_limits();
    return rt;
}

static void unload(JinviaRuntime *rt) {
    if (!rt) return;
    if (rt->vm) { j_vm_free(rt->vm); rt->vm = NULL; }
    if (rt->loaded) { j_free(&rt->mod); }
    memset(&rt->mod, 0, sizeof(rt->mod));
    memset(&rt->cap, 0, sizeof(rt->cap));
    rt->loaded = 0; rt->entry = 0;
}

void jinvia_free(JinviaRuntime *rt) {
    if (!rt) return;
    unload(rt);
    free(rt);
}

int jinvia_register_host(JinviaRuntime *rt, const char *name, JinviaHostFn fn, void *ud) {
    if (!rt || !name || !fn) return JINVIA_ERR_NULL;
    if (rt->nhosts >= JINVIA_MAX_HOST) return J_ERR_LIMIT;
    int idx = rt->nhosts;
    size_t n = strlen(name); if (n >= sizeof(rt->hosts[idx].name)) return J_ERR_LIMIT;
    memcpy(rt->hosts[idx].name, name, n + 1);
    rt->hosts[idx].fn = fn; rt->hosts[idx].ud = ud;
    rt->nhosts++;
    return idx;
}

int jinvia_set_time_budget(JinviaRuntime *rt, uint64_t ms) {
    if (!rt) return JINVIA_ERR_NULL;
    rt->lim_millis = ms;
    if (rt->vm) j_vm_set_time_budget(rt->vm, ms);
    return JINVIA_OK;
}

int jinvia_set_limits(JinviaRuntime *rt, uint64_t max_steps, uint32_t max_stack, uint32_t max_frames) {
    if (!rt) return JINVIA_ERR_NULL;
    JLimits l; l.max_steps = max_steps; l.max_stack = max_stack; l.max_frames = max_frames;
    rt->lim = l;
    if (rt->vm) j_vm_set_limits(rt->vm, l);
    return JINVIA_OK;
}

static int name_in_needs(const SrCap *cap, const char *name) {
    if (!cap || !name) return 0;
    for (int i = 0; i < cap->needs_count; i++)
        if (strcmp(cap->needs[i], name) == 0) return 1;
    return 0;
}

static int host_registered(JinviaRuntime *rt, const char *name) {
    for (int i = 0; i < rt->nhosts; i++) if (strcmp(rt->hosts[i].name, name) == 0) return 1;
    return 0;
}

int jinvia_load(JinviaRuntime *rt, const char *conf_text, const uint8_t *jcb, size_t jcb_len) {
    if (!rt || !conf_text || (!jcb && jcb_len)) return JINVIA_ERR_NULL;
    unload(rt);

    /* 1) 解析能力声明 */
    SrCap cap;
    if (sr_cap_parse(conf_text, &cap) != 0) return J_ERR_FMT;
    rt->cap = cap;

    /* 2) 权限闸：needs 必须全部被宿主注册（默认拒绝） */
    for (int i = 0; i < cap.needs_count; i++)
        if (!host_registered(rt, cap.needs[i])) return JINVIA_ERR_NEEDS;

    /* 3) 解析字节码 */
    if (j_parse(jcb, jcb_len, &rt->mod) != J_OK) { memset(&rt->mod,0,sizeof(rt->mod)); return J_ERR_FMT; }
    { int vrc = j_verify(&rt->mod); if (vrc != J_OK) { j_free(&rt->mod); memset(&rt->mod,0,sizeof(rt->mod)); return vrc; } }
    rt->loaded = 1;

    /* 4) 扫描 HOST 指令：索引必须 < 已注册宿主数，且该宿主名必须在该能力 needs 里声明过。
     *    这是铁律1「默认拒绝」的真正落地——否则能力可以声明 read:temp 却偷调 open:door。 */
    for (uint32_t f = 0; f < rt->mod.nfuncs; f++) {
        JInstr *ins; uint32_t n;
        if (j_decode(rt->mod.funcs[f].code, rt->mod.funcs[f].code_len, &ins, &n) != J_OK) { unload(rt); return J_ERR_FMT; }
        int bad = 0;
        for (uint32_t k = 0; k < n; k++) {
            if (ins[k].op != OP_HOST) continue;
            if (ins[k].arg >= (uint32_t)rt->nhosts) { bad = 1; break; }
            if (!name_in_needs(&rt->cap, rt->hosts[ins[k].arg].name)) { bad = 1; break; }
        }
        free(ins);
        if (bad) { unload(rt); return JINVIA_ERR_HOSTIDX; }
    }

    /* 5) 建 VM、接桥、设预算 */
    rt->vm = j_vm_new(&rt->mod);
    if (!rt->vm) { unload(rt); return J_ERR_OOM; }
    j_vm_set_host(rt->vm, host_bridge, rt);
    j_vm_set_limits(rt->vm, rt->lim);
    j_vm_set_time_budget(rt->vm, rt->lim_millis);

    /* 6) 入口：按能力名匹配 export，否则函数 0 */
    rt->entry = 0;
    for (uint32_t i = 0; i < rt->mod.nexports; i++)
        if (strcmp(rt->mod.export_names[i], cap.name) == 0) { rt->entry = rt->mod.export_idx[i]; break; }
    return JINVIA_OK;
}

int jinvia_call_args(JinviaRuntime *rt, const int64_t *args, uint32_t nargs, int64_t *result) {
    if (!rt) return JINVIA_ERR_NULL;
    if (!rt->loaded || !rt->vm) return JINVIA_ERR_NOLOAD;
    if (!result) return JINVIA_ERR_NULL;
    if (nargs && !args) return JINVIA_ERR_NULL;
    return j_vm_call(rt->vm, rt->entry, args, nargs, result);
}

int jinvia_call(JinviaRuntime *rt, int64_t *result) {
    return jinvia_call_args(rt, NULL, 0, result);
}

int jinvia_entry_nparams(JinviaRuntime *rt) {
    if (!rt || !rt->loaded || !rt->vm) return -1;
    if (rt->entry >= rt->mod.nfuncs) return -1;
    return (int)rt->mod.funcs[rt->entry].nparams;
}
uint32_t jinvia_mem_size(JinviaRuntime *rt) {
    if (!rt || !rt->loaded || !rt->vm) return 0;
    return j_vm_mem_size(rt->vm);
}
int jinvia_mem_write(JinviaRuntime *rt, uint32_t off, const void *src, uint32_t len) {
    if (!rt || !rt->loaded || !rt->vm) return JINVIA_ERR_NOLOAD;
    return j_vm_mem_write(rt->vm, off, src, len);
}

const char *jinvia_cap_name(JinviaRuntime *rt) {
    return (rt && rt->loaded) ? rt->cap.name : NULL;
}
int jinvia_host_count(JinviaRuntime *rt) { return rt ? rt->nhosts : 0; }
