/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include "jinvia/mcp.h"
#include "jinvia/jinvia.h"
#include "jinvia/cap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>

/* ---------- 可变字符串 ---------- */
typedef struct { char *p; size_t n, cap; } SB;
static int sbg(SB *b, size_t need) {
    if (b->n + need + 1 <= b->cap) return 0;
    size_t c = b->cap ? b->cap : 256;
    while (c < b->n + need + 1) c *= 2;
    char *t = (char *)realloc(b->p, c); if (!t) return -1;
    b->p = t; b->cap = c; return 0;
}
static int sba(SB *b, const char *s, size_t n) { if (sbg(b, n)) return -1; if (n) memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0; return 0; }
static int sbz(SB *b, const char *s) { return sba(b, s, strlen(s)); }
static int sbf(SB *b, const char *f, ...) {
    char tmp[512]; va_list ap; va_start(ap, f); int r = vsnprintf(tmp, sizeof tmp, f, ap); va_end(ap);
    if (r < 0) return -1;
    if ((size_t)r < sizeof tmp) return sba(b, tmp, (size_t)r);
    char *big = (char *)malloc((size_t)r + 1); if (!big) return -1;
    va_start(ap, f); vsnprintf(big, (size_t)r + 1, f, ap); va_end(ap);
    int rr = sba(b, big, (size_t)r); free(big); return rr;
}
static void sbfree(SB *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* ---------- 极简 JSON（值模型 + 递归下降解析 + 序列化） ---------- */
typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JK;
typedef struct JV JV;
struct JV { JK k; int b; double num; char *str; JV **v; char **key; int n; };

static void jfree(JV *v) {
    if (!v) return;
    if (v->str) free(v->str);
    for (int i = 0; i < v->n; i++) { if (v->v) jfree(v->v[i]); if (v->key) free(v->key[i]); }
    free(v->v); free(v->key); free(v);
}
static JV *jnew(JK k) { JV *v = (JV *)calloc(1, sizeof(JV)); if (v) v->k = k; return v; }

typedef struct { const char *s; size_t i, n; int err; } JP;
static void jws(JP *p) { while (p->i < p->n && (p->s[p->i] == ' ' || p->s[p->i] == '\t' || p->s[p->i] == '\r' || p->s[p->i] == '\n')) p->i++; }

static void put_utf8(SB *o, unsigned cp) {
    char b[4];
    if (cp < 0x80) { b[0] = (char)cp; sba(o, b, 1); }
    else if (cp < 0x800) { b[0] = (char)(0xC0 | (cp >> 6)); b[1] = (char)(0x80 | (cp & 0x3F)); sba(o, b, 2); }
    else if (cp < 0x10000) { b[0] = (char)(0xE0 | (cp >> 12)); b[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[2] = (char)(0x80 | (cp & 0x3F)); sba(o, b, 3); }
    else { b[0] = (char)(0xF0 | (cp >> 18)); b[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); b[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); b[3] = (char)(0x80 | (cp & 0x3F)); sba(o, b, 4); }
}
static int hex4(JP *p, unsigned *out) {
    if (p->i + 4 > p->n) return -1;
    unsigned v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p->s[p->i++]; int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return -1;
        v = v * 16 + (unsigned)d;
    }
    *out = v; return 0;
}
static char *jstr_parse(JP *p) {
    /* 前置已确认引号 */
    p->i++;
    SB o; memset(&o, 0, sizeof o);
    while (p->i < p->n) {
        char c = p->s[p->i++];
        if (c == '"') { if (!o.p) { o.p = (char *)malloc(1); if (o.p) o.p[0] = 0; } return o.p; }
        if (c == '\\') {
            if (p->i >= p->n) break;
            char e = p->s[p->i++];
            switch (e) {
                case '"': sbz(&o, "\""); break;
                case '\\': sbz(&o, "\\"); break;
                case '/': sbz(&o, "/"); break;
                case 'b': sbz(&o, "\b"); break;
                case 'f': sbz(&o, "\f"); break;
                case 'n': sbz(&o, "\n"); break;
                case 'r': sbz(&o, "\r"); break;
                case 't': sbz(&o, "\t"); break;
                case 'u': {
                    unsigned cp; if (hex4(p, &cp)) { sbfree(&o); p->err = 1; return NULL; }
                    if (cp >= 0xD800 && cp <= 0xDBFF && p->i + 6 <= p->n && p->s[p->i] == '\\' && p->s[p->i + 1] == 'u') {
                        size_t save = p->i; p->i += 2; unsigned lo;
                        if (!hex4(p, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        else p->i = save;
                    }
                    put_utf8(&o, cp);
                    break;
                }
                default: sbfree(&o); p->err = 1; return NULL;
            }
        } else {
            sba(&o, &c, 1);
        }
    }
    sbfree(&o); p->err = 1; return NULL;
}

static JV *jval(JP *p);
static JV *jarr_parse(JP *p) {
    p->i++; JV *a = jnew(J_ARR); if (!a) { p->err = 1; return NULL; }
    jws(p);
    if (p->i < p->n && p->s[p->i] == ']') { p->i++; return a; }
    for (;;) {
        JV *e = jval(p); if (!e) { jfree(a); return NULL; }
        JV **nv = (JV **)realloc(a->v, (size_t)(a->n + 1) * sizeof(JV *)); if (!nv) { jfree(e); jfree(a); p->err = 1; return NULL; }
        a->v = nv; a->v[a->n++] = e;
        jws(p);
        if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
        if (p->i < p->n && p->s[p->i] == ']') { p->i++; return a; }
        jfree(a); p->err = 1; return NULL;
    }
}
static JV *jobj_parse(JP *p) {
    p->i++; JV *o = jnew(J_OBJ); if (!o) { p->err = 1; return NULL; }
    jws(p);
    if (p->i < p->n && p->s[p->i] == '}') { p->i++; return o; }
    for (;;) {
        jws(p);
        if (p->i >= p->n || p->s[p->i] != '"') { jfree(o); p->err = 1; return NULL; }
        char *k = jstr_parse(p); if (!k) { jfree(o); return NULL; }
        jws(p);
        if (p->i >= p->n || p->s[p->i] != ':') { free(k); jfree(o); p->err = 1; return NULL; }
        p->i++;
        JV *v = jval(p); if (!v) { free(k); jfree(o); return NULL; }
        JV **nv = (JV **)realloc(o->v, (size_t)(o->n + 1) * sizeof(JV *));
        char **nk = (char **)realloc(o->key, (size_t)(o->n + 1) * sizeof(char *));
        if (!nv || !nk) { free(k); jfree(v); free(nv); free(nk); jfree(o); p->err = 1; return NULL; }
        o->v = nv; o->key = nk; o->key[o->n] = k; o->v[o->n] = v; o->n++;
        jws(p);
        if (p->i < p->n && p->s[p->i] == ',') { p->i++; continue; }
        if (p->i < p->n && p->s[p->i] == '}') { p->i++; return o; }
        jfree(o); p->err = 1; return NULL;
    }
}
static JV *jval(JP *p) {
    jws(p);
    if (p->i >= p->n) { p->err = 1; return NULL; }
    char c = p->s[p->i];
    if (c == '{') return jobj_parse(p);
    if (c == '[') return jarr_parse(p);
    if (c == '"') { char *s = jstr_parse(p); if (!s) return NULL; JV *v = jnew(J_STR); if (!v) { free(s); p->err = 1; return NULL; } v->str = s; return v; }
    if (c == 't' && p->i + 4 <= p->n && !strncmp(p->s + p->i, "true", 4)) { p->i += 4; JV *v = jnew(J_BOOL); if (v) v->b = 1; return v; }
    if (c == 'f' && p->i + 5 <= p->n && !strncmp(p->s + p->i, "false", 5)) { p->i += 5; JV *v = jnew(J_BOOL); if (v) v->b = 0; return v; }
    if (c == 'n' && p->i + 4 <= p->n && !strncmp(p->s + p->i, "null", 4)) { p->i += 4; return jnew(J_NULL); }
    /* number */
    { size_t st = p->i; if (p->s[p->i] == '-' || p->s[p->i] == '+') p->i++;
      int any = 0;
      while (p->i < p->n && ((p->s[p->i] >= '0' && p->s[p->i] <= '9') || p->s[p->i] == '.' || p->s[p->i] == 'e' || p->s[p->i] == 'E' || p->s[p->i] == '+' || p->s[p->i] == '-')) { p->i++; any = 1; }
      if (!any) { p->err = 1; return NULL; }
      char buf[64]; size_t L = p->i - st; if (L >= sizeof buf) L = sizeof buf - 1;
      memcpy(buf, p->s + st, L); buf[L] = 0;
      JV *v = jnew(J_NUM); if (!v) { p->err = 1; return NULL; } v->num = strtod(buf, NULL); return v; }
}
static JV *jparse(const char *s, size_t n) {
    JP p; p.s = s; p.i = 0; p.n = n; p.err = 0;
    JV *v = jval(&p);
    if (p.err) { jfree(v); return NULL; }
    jws(&p);
    if (p.i != p.n) { jfree(v); return NULL; }
    return v;
}
static JV *jget(JV *o, const char *k) {
    if (!o || o->k != J_OBJ) return NULL;
    for (int i = 0; i < o->n; i++) if (!strcmp(o->key[i], k)) return o->v[i];
    return NULL;
}
static const char *jget_str(JV *o, const char *k) { JV *v = jget(o, k); return (v && v->k == J_STR) ? v->str : NULL; }

/* 序列化 JSON 字符串（带引号） */
static void jesc(SB *o, const char *s) {
    sbz(o, "\"");
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) { /* 防御 NULL：缺失字段按空串序列化 */
        unsigned char c = *p;
        switch (c) {
            case '"': sbz(o, "\\\""); break;
            case '\\': sbz(o, "\\\\"); break;
            case '\n': sbz(o, "\\n"); break;
            case '\r': sbz(o, "\\r"); break;
            case '\t': sbz(o, "\\t"); break;
            default:
                if (c < 0x20) sbf(o, "\\u%04x", c);
                else sba(o, (const char *)p, 1);
        }
    }
    sbz(o, "\"");
}
/* 原样序列化一个 JV（主要用于回显 id） */
static void jemit(SB *o, JV *v) {
    if (!v) { sbz(o, "null"); return; }
    switch (v->k) {
        case J_NULL: sbz(o, "null"); break;
        case J_BOOL: sbz(o, v->b ? "true" : "false"); break;
        case J_NUM: if (v->num == (double)(long long)v->num) sbf(o, "%lld", (long long)v->num); else sbf(o, "%g", v->num); break;
        case J_STR: jesc(o, v->str); break;
        case J_ARR: { sbz(o, "["); for (int i = 0; i < v->n; i++) { if (i) sbz(o, ","); jemit(o, v->v[i]); } sbz(o, "]"); break; }
        case J_OBJ: { sbz(o, "{"); for (int i = 0; i < v->n; i++) { if (i) sbz(o, ","); jesc(o, v->key[i]); sbz(o, ":"); jemit(o, v->v[i]); } sbz(o, "}"); break; }
    }
}

/* ---------- 能力包扫描 = 工具注册表 ---------- */
typedef struct { char *name; char *does; char *dir; SrCap cap; } Tool;
typedef struct { Tool *t; int n; } ToolSet;

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *b = (char *)malloc((size_t)sz + 1); if (!b) { fclose(f); return NULL; }
    size_t r = fread(b, 1, (size_t)sz, f); fclose(f); b[r] = 0; return b;
}
static void toolset_free(ToolSet *ts) { for (int i = 0; i < ts->n; i++) { free(ts->t[i].name); free(ts->t[i].does); free(ts->t[i].dir); } free(ts->t); ts->t = NULL; ts->n = 0; }

static ToolSet load_tools(const char *dir) {
    ToolSet ts; ts.t = NULL; ts.n = 0;
    DIR *d = opendir(dir); if (!d) return ts;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char cpath[1200]; snprintf(cpath, sizeof cpath, "%s/%s/capability.conf", dir, de->d_name);
        char *conf = read_all(cpath); if (!conf) continue;
        SrCap cap;
        if (sr_cap_parse(conf, &cap) == 0 && cap.name[0]) {
            Tool t; t.name = strdup(cap.name); t.does = strdup(cap.does[0] ? cap.does : "(no description)");
            char dp[1200]; snprintf(dp, sizeof dp, "%s/%s", dir, de->d_name); t.dir = strdup(dp);
            t.cap = cap;
            Tool *nt = (Tool *)realloc(ts.t, (size_t)(ts.n + 1) * sizeof(Tool));
            if (nt && t.name && t.does && t.dir) { ts.t = nt; ts.t[ts.n++] = t; }
            else { free(t.name); free(t.does); free(t.dir); free(nt); }
        }
        free(conf);
    }
    closedir(d);
    return ts;
}

/* ---------- 沙箱执行一个 tool ---------- */
static int64_t h_temp(void *u, int64_t a) { (void)u; (void)a; return 34; }
static int64_t h_alarm(void *u, int64_t a) { (void)u; (void)a; fprintf(stderr, "[jinvia-host] alarm!\n"); return 0; }
static int64_t h_motion(void *u, int64_t a) { (void)u; (void)a; return 1; }
static int64_t h_door(void *u, int64_t a) { (void)u; (void)a; fprintf(stderr, "[jinvia-host] open door\n"); return 0; }

static const char *errname(int rc) {
    switch (rc) {
        case JINVIA_OK: return "OK";
        case J_ERR_FMT: return "format/verify failed";
        case J_ERR_VM: return "runtime error (bounds/budget/div0)";
        case JINVIA_ERR_NEEDS: return "refused: undeclared host capability (default deny)";
        case JINVIA_ERR_HOSTIDX: return "refused: HOST index out of range";
        case JINVIA_ERR_NOLOAD: return "not loaded";
        case JINVIA_ERR_NULL: return "null arg";
        default: return "error";
    }
}
/* 是否布尔类型的参数声明 */
static int is_bool_type(const char *t) { return t && (strcmp(t, "bool") == 0 || strcmp(t, "boolean") == 0); }
/* 是否字符串类型的参数声明 */
static int is_string_type(const char *t) { return t && (strcmp(t, "string") == 0 || strcmp(t, "str") == 0); }

/* 把 JSON 参数值转成 VM 用的 i64；成功 0。
 * -1 缺失（含 null），-2 超出 i64 范围，-3 类型不符（int 要求整值 / bool 要求布尔）。 */
static int jv_to_i64(JV *v, int is_bool, int64_t *out) {
    if (!v || v->k == J_NULL || !out) return -1;
    if (is_bool) {
        if (v->k == J_BOOL) { *out = v->b ? 1 : 0; return 0; }
        if (v->k == J_NUM && (v->num == 0.0 || v->num == 1.0)) { *out = (int64_t)v->num; return 0; }
        return -3;
    }
    if (v->k == J_BOOL) { *out = v->b ? 1 : 0; return 0; }  /* 宽松：布尔当 0/1 */
    if (v->k != J_NUM) return -3;
    double d = v->num;
    if (!(d >= -9223372036854775808.0 && d < 9223372036854775808.0)) return -2;
    int64_t iv = (int64_t)d;
    if ((double)iv != d) return -3;
    *out = iv;
    return 0;
}

/* 按 params 声明生成 MCP inputSchema（integer / boolean；全部 required）。 */
static void emit_input_schema(SB *o, const SrCap *cap) {
    sbz(o, "{\"type\":\"object\",\"properties\":{");
    for (int i = 0; i < cap->params_count; i++) {
        if (i) sbz(o, ",");
        jesc(o, cap->params[i].name);
        sbz(o, is_string_type(cap->params[i].type) ? ":{\"type\":\"string\"}" : (is_bool_type(cap->params[i].type) ? ":{\"type\":\"boolean\"}" : ":{\"type\":\"integer\"}"));
    }
    sbz(o, "}");
    if (cap->params_count > 0) {
        sbz(o, ",\"required\":[");
        for (int i = 0; i < cap->params_count; i++) { if (i) sbz(o, ","); jesc(o, cap->params[i].name); }
        sbz(o, "]");
    }
    sbz(o, ",\"additionalProperties\":false}");
}

/* 运行一个 tool；成功返回 0 并写 *res；失败返回负码并把消息写进 errbuf */
static int run_tool(Tool *t, JV *arguments, int64_t *res, char *errbuf, size_t errcap) {
    char cp[1300], jp[1300];
    snprintf(cp, sizeof cp, "%s/capability.conf", t->dir);
    char *conf = read_all(cp); if (!conf) { snprintf(errbuf, errcap, "cannot read capability.conf"); return -1; }
    SrCap cap;
    if (sr_cap_parse(conf, &cap) != 0) { free(conf); snprintf(errbuf, errcap, "bad capability.conf"); return -1; }
    snprintf(jp, sizeof jp, "%s/%s", t->dir, cap.entry[0] ? cap.entry : "main.jcb");
    FILE *jf = fopen(jp, "rb");
    if (!jf) { free(conf); snprintf(errbuf, errcap, "cannot read bytecode"); return -1; }
    fseek(jf, 0, SEEK_END); long sz = ftell(jf); fseek(jf, 0, SEEK_SET);
    unsigned char *jcb = (unsigned char *)malloc((size_t)(sz > 0 ? sz : 1));
    size_t jl = jcb ? fread(jcb, 1, (size_t)(sz > 0 ? sz : 0), jf) : 0;
    fclose(jf);

    int ret = -1;
    JinviaRuntime *rt = jinvia_new();
    if (!rt) { snprintf(errbuf, errcap, "out of memory"); goto done; }
    jinvia_register_host(rt, "read:temp", h_temp, NULL);     /* HOST 0 */
    jinvia_register_host(rt, "call:alarm", h_alarm, NULL);   /* HOST 1 */
    jinvia_register_host(rt, "read:motion", h_motion, NULL); /* HOST 2 */
    jinvia_register_host(rt, "open:door", h_door, NULL);     /* HOST 3 */
    jinvia_set_limits(rt, 10000000ULL, 4096, 256);
    jinvia_set_time_budget(rt, 2000);
    int rc = jinvia_load(rt, conf, jcb, jl);
    if (rc != JINVIA_OK) { snprintf(errbuf, errcap, "%s", errname(rc)); jinvia_free(rt); goto done; }

    /* 从 JSON arguments 按 params 声明顺序构造 VM 参数向量：
     *   int/bool -> 1 个 i64 槽；string -> 2 个槽（ptr, len，字节写入线性内存）。 */
    int64_t argv[SR_PARAMS_MAX * 2];
    uint32_t nargs = 0;
    uint32_t mem_off = 0, msize = jinvia_mem_size(rt);
    for (int i = 0; i < cap.params_count; i++) {
        const char *pn = cap.params[i].name;
        const char *pt = cap.params[i].type;
        JV *pv = jget(arguments, pn);
        if (is_string_type(pt)) {
            if (!pv || pv->k == J_NULL) { snprintf(errbuf, errcap, "missing required argument '%s'", pn); jinvia_free(rt); goto done; }
            if (pv->k != J_STR) { snprintf(errbuf, errcap, "argument '%s' must be a string", pn); jinvia_free(rt); goto done; }
            const char *str = pv->str ? pv->str : "";
            size_t slen = strlen(str);
            if (msize == 0) { snprintf(errbuf, errcap, "string argument '%s' needs the tool to declare linear memory (.memory N)", pn); jinvia_free(rt); goto done; }
            uint32_t off = (mem_off + 7u) & ~7u;
            if ((uint64_t)off + (uint64_t)slen + 1u > (uint64_t)msize) { snprintf(errbuf, errcap, "string argument '%s' exceeds the tool's linear memory", pn); jinvia_free(rt); goto done; }
            if (jinvia_mem_write(rt, off, str, (uint32_t)slen) != 0) { snprintf(errbuf, errcap, "failed to inject string argument '%s'", pn); jinvia_free(rt); goto done; }
            { uint8_t nul = 0; jinvia_mem_write(rt, off + (uint32_t)slen, &nul, 1); }
            argv[nargs++] = (int64_t)off;
            argv[nargs++] = (int64_t)slen;
            mem_off = off + (uint32_t)slen + 1u;
        } else {
            int conv = jv_to_i64(pv, is_bool_type(pt), &argv[nargs]);
            if (conv == -1) { snprintf(errbuf, errcap, "missing required argument '%s'", pn); jinvia_free(rt); goto done; }
            if (conv == -2) { snprintf(errbuf, errcap, "argument '%s' out of range", pn); jinvia_free(rt); goto done; }
            if (conv == -3) { snprintf(errbuf, errcap, "argument '%s' must be %s", pn, is_bool_type(pt) ? "a boolean" : "an integer"); jinvia_free(rt); goto done; }
            nargs++;
        }
    }
    { int expect = jinvia_entry_nparams(rt);
      if (expect != (int)nargs) { snprintf(errbuf, errcap, "tool params occupy %u VM slot(s) but the entry function expects %d", nargs, expect); jinvia_free(rt); goto done; } }

    rc = jinvia_call_args(rt, argv, nargs, res);
    if (rc != JINVIA_OK) { snprintf(errbuf, errcap, "%s", errname(rc)); jinvia_free(rt); goto done; }
    jinvia_free(rt);
    ret = 0;
done:
    free(conf); free(jcb); return ret;
}

/* ---------- 生成响应 ---------- */
static void emit_error(FILE *out, JV *id, int code, const char *msg) {
    SB o; memset(&o, 0, sizeof o);
    sbz(&o, "{\"jsonrpc\":\"2.0\",\"id\":"); jemit(&o, id);
    sbf(&o, ",\"error\":{\"code\":%d,\"message\":", code); jesc(&o, msg); sbz(&o, "}}\n");
    fwrite(o.p, 1, o.n, out); fflush(out); sbfree(&o);
}

static void handle_request(const char *tools_dir, JV *req, FILE *out) {
    JV *idv = jget(req, "id");
    const char *method = jget_str(req, "method");
    if (!method) { emit_error(out, idv, -32600, "invalid request"); return; }

    /* notifications（无 id）：不回应 */
    if (!idv) return;

    if (!strcmp(method, "initialize")) {
        SB o; memset(&o, 0, sizeof o);
        sbz(&o, "{\"jsonrpc\":\"2.0\",\"id\":"); jemit(&o, idv);
        sbz(&o, ",\"result\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"jinvia\",\"version\":\"0.1.0\"}}}\n");
        fwrite(o.p, 1, o.n, out); fflush(out); sbfree(&o);
        return;
    }
    if (!strcmp(method, "ping")) {
        SB o; memset(&o, 0, sizeof o);
        sbz(&o, "{\"jsonrpc\":\"2.0\",\"id\":"); jemit(&o, idv); sbz(&o, ",\"result\":{}}\n");
        fwrite(o.p, 1, o.n, out); fflush(out); sbfree(&o); return;
    }
    if (!strcmp(method, "tools/list")) {
        ToolSet ts = load_tools(tools_dir);
        SB o; memset(&o, 0, sizeof o);
        sbz(&o, "{\"jsonrpc\":\"2.0\",\"id\":"); jemit(&o, idv); sbz(&o, ",\"result\":{\"tools\":[");
        for (int i = 0; i < ts.n; i++) {
            if (i) sbz(&o, ",");
            sbz(&o, "{\"name\":"); jesc(&o, ts.t[i].name);
            sbz(&o, ",\"description\":"); jesc(&o, ts.t[i].does);
            sbz(&o, ",\"inputSchema\":"); emit_input_schema(&o, &ts.t[i].cap); sbz(&o, "}");
        }
        sbz(&o, "]}}\n");
        fwrite(o.p, 1, o.n, out); fflush(out); sbfree(&o); toolset_free(&ts);
        return;
    }
    if (!strcmp(method, "tools/call")) {
        JV *params = jget(req, "params");
        const char *name = params ? jget_str(params, "name") : NULL;
        if (!name) { emit_error(out, idv, -32602, "missing tool name"); return; }
        JV *arguments = params ? jget(params, "arguments") : NULL;
        ToolSet ts = load_tools(tools_dir);
        Tool *hit = NULL;
        for (int i = 0; i < ts.n; i++) if (!strcmp(ts.t[i].name, name)) { hit = &ts.t[i]; break; }
        if (!hit) { emit_error(out, idv, -32602, "unknown tool"); toolset_free(&ts); return; }
        int64_t res = 0; char eb[256] = {0};
        int rc = run_tool(hit, arguments, &res, eb, sizeof eb);
        SB o; memset(&o, 0, sizeof o);
        sbz(&o, "{\"jsonrpc\":\"2.0\",\"id\":"); jemit(&o, idv); sbz(&o, ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":");
        if (rc == 0) { char txt[64]; snprintf(txt, sizeof txt, "result = %lld", (long long)res); jesc(&o, txt); }
        else { char txt[320]; snprintf(txt, sizeof txt, "error: %s", eb[0] ? eb : "failed"); jesc(&o, txt); }
        sbf(&o, "}],\"isError\":%s}}\n", rc == 0 ? "false" : "true");
        fwrite(o.p, 1, o.n, out); fflush(out); sbfree(&o); toolset_free(&ts);
        return;
    }
    emit_error(out, idv, -32601, "method not found");
}

/* 可移植的行读取：替代 POSIX 独有 getline（Windows 无）。
   成功读一行返回字节数(>=0)，EOF 返回 -1，内存不足返回 -2。 */
static long read_line(char **line, size_t *cap, FILE *in) {
    size_t n = 0; int c; int got = 0;
    if (!line || !cap || !in) return -2;
    if (*line == NULL || *cap == 0) {
        *cap = 256; *line = (char *)malloc(*cap);
        if (!*line) { *cap = 0; return -2; }
    }
    while ((c = fgetc(in)) != EOF) {
        got = 1;
        if (n + 1 >= *cap) {
            size_t nc = *cap * 2; char *t = (char *)realloc(*line, nc);
            if (!t) return -2;
            *line = t; *cap = nc;
        }
        (*line)[n++] = (char)c;
        if (c == '\n') break;
    }
    if (!got && c == EOF) return -1;
    (*line)[n] = '\0';
    return (long)n;
}

int j_mcp_handle_line(const char *tools_dir, const char *line, size_t len, FILE *out) {
    JV *req = jparse(line, len);
    if (!req) { emit_error(out, NULL, -32700, "parse error"); return 0; }
    if (req->k != J_OBJ) { emit_error(out, NULL, -32600, "invalid request"); jfree(req); return 0; }
    handle_request(tools_dir, req, out);
    jfree(req);
    return 0;
}

int j_mcp_serve(const char *tools_dir, FILE *in, FILE *out) {
    if (!tools_dir || !in || !out) return -1;
    char *line = NULL; size_t cap = 0; long n;
    while ((n = read_line(&line, &cap, in)) >= 0) {
        /* 去掉行尾换行 */
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) n--;
        if (n == 0) continue;
        j_mcp_handle_line(tools_dir, line, (size_t)n, out);
    }
    free(line);
    return 0;
}
