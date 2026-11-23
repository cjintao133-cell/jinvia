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
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
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
typedef struct { char *name; char *does; char *dir; } Tool;
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
/* 运行一个 tool；成功返回 0 并写 *res；失败返回负码并把消息写进 errbuf */
static int run_tool(Tool *t, int64_t *res, char *errbuf, size_t errcap) {
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
    int rc = jinvia_load(rt, conf, jcb, jl);
    if (rc != JINVIA_OK) { snprintf(errbuf, errcap, "%s", errname(rc)); jinvia_free(rt); goto done; }
    rc = jinvia_call(rt, res);
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
            sbz(&o, ",\"inputSchema\":{\"type\":\"object\",\"properties\":{},\"additionalProperties\":false}}");
        }
        sbz(&o, "]}}\n");
        fwrite(o.p, 1, o.n, out); fflush(out); sbfree(&o); toolset_free(&ts);
        return;
    }
    if (!strcmp(method, "tools/call")) {
        JV *params = jget(req, "params");
        const char *name = params ? jget_str(params, "name") : NULL;
        if (!name) { emit_error(out, idv, -32602, "missing tool name"); return; }
        ToolSet ts = load_tools(tools_dir);
        Tool *hit = NULL;
        for (int i = 0; i < ts.n; i++) if (!strcmp(ts.t[i].name, name)) { hit = &ts.t[i]; break; }
        if (!hit) { emit_error(out, idv, -32602, "unknown tool"); toolset_free(&ts); return; }
        int64_t res = 0; char eb[256] = {0};
        int rc = run_tool(hit, &res, eb, sizeof eb);
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
    char *line = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&line, &cap, in)) != -1) {
        /* 去掉行尾换行 */
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) n--;
        if (n == 0) continue;
        j_mcp_handle_line(tools_dir, line, (size_t)n, out);
    }
    free(line);
    return 0;
}
