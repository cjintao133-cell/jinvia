/* SPDX-License-Identifier: Apache-2.0 */
/* Jinvia C 子集编译器 —— 把一小段类 C 源码编译成 JCB 汇编文本(.jcbasm)，
 * 再由 j_asm() 汇编成 JCB 字节码。给作者一个"写 C、不写汇编"的入口。
 *
 * 支持的子集（够写真实小工具）：
 *   - 类型：int（64 位有符号）全局变量、局部变量
 *   - 函数：int f(int a, int b, ...) { ... }，带参数与返回值，支持递归
 *   - 表达式：int 字面量(十进制/0x)、变量、函数调用、+ - * / %、
 *             == != < > <= >=、&& || !、一元负、括号
 *   - 语句：声明(可带初始化)、赋值、if/else、while、return、表达式语句、块
 *   - extern：extern int name(int); 按声明顺序映射到 HOST 0,1,2...
 *
 * 未支持（诚实声明）：指针、数组、结构体、float、for、break/continue、复合赋值。
 *
 * 产物是"汇编文本"，交给已充分测试的 j_asm 生成字节码——前端与编码分离，
 * 生成的 .jcbasm 也便于人工审计。纯 C11，零第三方依赖。 */
#include "jinvia/cc.h"
#include "jinvia/jcb.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

/* ===================== 一次性内存池（arena） ===================== */
typedef struct ABlk { struct ABlk *next; size_t cap, used; char data[]; } ABlk;
typedef struct { ABlk *head; } Arena;

static void *ar_alloc(Arena *a, size_t n) {
    n = (n + 15u) & ~(size_t)15u;
    if (n == 0) n = 16;
    if (!a->head || a->head->used + n > a->head->cap) {
        size_t cap = n > 65536u ? n : 65536u;
        ABlk *b = (ABlk *)malloc(sizeof(ABlk) + cap);
        if (!b) return NULL;
        b->next = a->head; b->cap = cap; b->used = 0; a->head = b;
    }
    void *p = a->head->data + a->head->used;
    a->head->used += n;
    return p;
}
static void ar_free(Arena *a) { ABlk *b = a->head; while (b) { ABlk *nx = b->next; free(b); b = nx; } a->head = NULL; }
static char *ar_strdup(Arena *a, const char *s) { size_t n = strlen(s) + 1; char *p = (char *)ar_alloc(a, n); if (p) memcpy(p, s, n); return p; }

/* ===================== 可变字符串 ===================== */
typedef struct { char *p; size_t n, cap; } SB;
static int sb_putn(SB *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap : 256;
        while (c < b->n + n + 1) c *= 2;
        char *t = (char *)realloc(b->p, c);
        if (!t) return -1;
        b->p = t; b->cap = c;
    }
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n; b->p[b->n] = '\0';
    return 0;
}
static int sb_puts(SB *b, const char *s) { return sb_putn(b, s, strlen(s)); }
static int sb_printf(SB *b, const char *f, ...) {
    char tmp[512]; va_list ap;
    va_start(ap, f); int r = vsnprintf(tmp, sizeof tmp, f, ap); va_end(ap);
    if (r < 0) return -1;
    if ((size_t)r < sizeof tmp) return sb_putn(b, tmp, (size_t)r);
    char *big = (char *)malloc((size_t)r + 1);
    if (!big) return -1;
    va_start(ap, f); vsnprintf(big, (size_t)r + 1, f, ap); va_end(ap);
    int rr = sb_putn(b, big, (size_t)r); free(big); return rr;
}
static void sb_free(SB *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* ===================== 词法 ===================== */
typedef enum {
    T_EOF = 0, T_NUM, T_IDENT,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_SEMI, T_COMMA,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_EQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_ANDAND, T_OROR, T_BANG, T_ASSIGN,
    T_KW_INT, T_KW_IF, T_KW_ELSE, T_KW_WHILE, T_KW_RETURN, T_KW_EXTERN
} TokType;

typedef struct { TokType t; int64_t num; char *name; size_t pos; } Tok;
typedef struct { Tok *v; int n, cap; } TokVec;

static int tv_push(TokVec *tv, Tok t) {
    if (tv->n == tv->cap) {
        int nc = tv->cap ? tv->cap * 2 : 128;
        Tok *nv = (Tok *)realloc(tv->v, (size_t)nc * sizeof(Tok));
        if (!nv) return -1;
        tv->v = nv; tv->cap = nc;
    }
    tv->v[tv->n++] = t;
    return 0;
}

static int keyword(const char *s, TokType *out) {
    static const struct { const char *w; TokType t; } K[] = {
        {"int", T_KW_INT}, {"if", T_KW_IF}, {"else", T_KW_ELSE},
        {"while", T_KW_WHILE}, {"return", T_KW_RETURN}, {"extern", T_KW_EXTERN}
    };
    for (size_t i = 0; i < sizeof K / sizeof K[0]; i++)
        if (strcmp(s, K[i].w) == 0) { *out = K[i].t; return 1; }
    return 0;
}

/* 返回 0 成功；-1 失败并把错误写进 err */
static int lex_source(const char *src, Arena *a, TokVec *tv, char *err, size_t errcap) {
    size_t i = 0;
    while (src[i]) {
        char ch = src[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') { i++; continue; }
        if (ch == '/' && src[i + 1] == '/') { while (src[i] && src[i] != '\n') i++; continue; }
        if (ch == '/' && src[i + 1] == '*') {
            i += 2;
            while (src[i] && !(src[i] == '*' && src[i + 1] == '/')) i++;
            if (src[i]) i += 2;
            continue;
        }
        Tok tk; memset(&tk, 0, sizeof tk); tk.pos = i;
        if (isdigit((unsigned char)ch)) {
            size_t st = i; int base = 10;
            if (ch == '0' && (src[i + 1] == 'x' || src[i + 1] == 'X')) { base = 16; i += 2; while (isxdigit((unsigned char)src[i])) i++; }
            else { while (isdigit((unsigned char)src[i])) i++; }
            char buf[80]; size_t L = i - st;
            if (L == 0 || L >= sizeof buf) { snprintf(err, errcap, "数字字面量过长(位置 %llu)", (unsigned long long)st); return -1; }
            memcpy(buf, src + st, L); buf[L] = '\0';
            char *endp = NULL;
            long long v = strtoll(buf, &endp, base);
            if (endp && *endp) { snprintf(err, errcap, "非法数字 '%s'(位置 %llu)", buf, (unsigned long long)st); return -1; }
            tk.t = T_NUM; tk.num = (int64_t)v;
            if (tv_push(tv, tk)) { snprintf(err, errcap, "内存不足"); return -1; }
            continue;
        }
        if (isalpha((unsigned char)ch) || ch == '_') {
            size_t st = i;
            while (isalnum((unsigned char)src[i]) || src[i] == '_') i++;
            size_t L = i - st;
            if (L >= 200) { snprintf(err, errcap, "标识符过长(位置 %llu)", (unsigned long long)st); return -1; }
            char buf[200]; memcpy(buf, src + st, L); buf[L] = '\0';
            TokType kw;
            if (keyword(buf, &kw)) { tk.t = kw; }
            else { tk.t = T_IDENT; tk.name = ar_strdup(a, buf); if (!tk.name) { snprintf(err, errcap, "内存不足"); return -1; } }
            if (tv_push(tv, tk)) { snprintf(err, errcap, "内存不足"); return -1; }
            continue;
        }
        /* 运算符 */
        char c2 = src[i + 1];
        if (ch == '=' && c2 == '=') { tk.t = T_EQ; i += 2; }
        else if (ch == '!' && c2 == '=') { tk.t = T_NE; i += 2; }
        else if (ch == '<' && c2 == '=') { tk.t = T_LE; i += 2; }
        else if (ch == '>' && c2 == '=') { tk.t = T_GE; i += 2; }
        else if (ch == '&' && c2 == '&') { tk.t = T_ANDAND; i += 2; }
        else if (ch == '|' && c2 == '|') { tk.t = T_OROR; i += 2; }
        else {
            switch (ch) {
                case '(': tk.t = T_LPAREN; break;
                case ')': tk.t = T_RPAREN; break;
                case '{': tk.t = T_LBRACE; break;
                case '}': tk.t = T_RBRACE; break;
                case ';': tk.t = T_SEMI; break;
                case ',': tk.t = T_COMMA; break;
                case '+': tk.t = T_PLUS; break;
                case '-': tk.t = T_MINUS; break;
                case '*': tk.t = T_STAR; break;
                case '/': tk.t = T_SLASH; break;
                case '%': tk.t = T_PERCENT; break;
                case '<': tk.t = T_LT; break;
                case '>': tk.t = T_GT; break;
                case '!': tk.t = T_BANG; break;
                case '=': tk.t = T_ASSIGN; break;
                default: snprintf(err, errcap, "无法识别的字符 '%c'(位置 %llu)", ch, (unsigned long long)i); return -1;
            }
            i++;
        }
        if (tv_push(tv, tk)) { snprintf(err, errcap, "内存不足"); return -1; }
    }
    Tok eof; memset(&eof, 0, sizeof eof); eof.t = T_EOF; eof.pos = i;
    if (tv_push(tv, eof)) { snprintf(err, errcap, "内存不足"); return -1; }
    return 0;
}

/* ===================== AST ===================== */
typedef enum { E_NUM, E_VAR, E_BIN, E_UN, E_CALL } EK;
typedef struct Expr Expr;
struct Expr {
    EK k;
    int64_t num;
    char *name;
    TokType op;          /* 二元/一元运算符 */
    Expr *a, *b;
    Expr **args; int nargs;
};

typedef enum { S_DECL, S_ASSIGN, S_IF, S_WHILE, S_RETURN, S_EXPR, S_BLOCK, S_EMPTY } SK;
typedef struct Stmt Stmt;
struct Stmt {
    SK k;
    char *name;
    Expr *e;       /* 赋值右值 / 返回表达式 / 表达式语句 / 条件 */
    Expr *cond;
    Stmt *then, *els;
    Stmt **list; int n;
};

typedef struct { char *name; char **params; int nparams; Stmt *body; } Func;
typedef struct { char *name; int64_t init; } Global;
typedef struct { char *name; } Extern;

typedef struct {
    Tok *t; int n, i;
    Arena *a;
    const char *src;
    char err[256];
    Func *funcs; int nfuncs, fcap;
    Global *globs; int nglobs, gcap;
    Extern *exts; int nexts, ecap;
} P;

static void p_err(P *p, const char *fmt, ...) {
    if (p->err[0]) return;
    va_list ap; va_start(ap, fmt); vsnprintf(p->err, sizeof p->err, fmt, ap); va_end(ap);
}
static TokType pk(P *p) { return p->t[p->i].t; }
static Tok *cur(P *p) { return &p->t[p->i]; }
static void adv(P *p) { if (p->t[p->i].t != T_EOF) p->i++; }

static Expr *mk(P *p, EK k) { Expr *e = (Expr *)ar_alloc(p->a, sizeof(Expr)); if (e) memset(e, 0, sizeof *e), e->k = k; else p_err(p, "内存不足"); return e; }
static Stmt *mks(P *p, SK k) { Stmt *s = (Stmt *)ar_alloc(p->a, sizeof(Stmt)); if (s) memset(s, 0, sizeof *s), s->k = k; else p_err(p, "内存不足"); return s; }

/* 解析 */
static Expr *parse_expr(P *p);
static Stmt *parse_stmt(P *p);

static Expr *parse_primary(P *p) {
    TokType tt = pk(p);
    if (tt == T_NUM) { Expr *e = mk(p, E_NUM); if (!e) return NULL; e->num = cur(p)->num; adv(p); return e; }
    if (tt == T_IDENT) {
        char *name = cur(p)->name; adv(p);
        if (pk(p) == T_LPAREN) {
            adv(p);
            Expr *e = mk(p, E_CALL); if (!e) return NULL; e->name = name;
            Expr **args = NULL; int n = 0, cap = 0;
            if (pk(p) != T_RPAREN) {
                for (;;) {
                    Expr *arg = parse_expr(p); if (!arg) return NULL;
                    if (n == cap) { int nc = cap ? cap * 2 : 4; Expr **na = (Expr **)ar_alloc(p->a, (size_t)nc * sizeof(Expr *)); if (!na) { p_err(p, "内存不足"); return NULL; } if (n) memcpy(na, args, (size_t)n * sizeof(Expr *)); args = na; cap = nc; }
                    args[n++] = arg;
                    if (pk(p) == T_COMMA) { adv(p); continue; }
                    break;
                }
            }
            if (pk(p) != T_RPAREN) { p_err(p, "函数调用缺少 ')'"); return NULL; }
            adv(p); e->args = args; e->nargs = n; return e;
        }
        Expr *e = mk(p, E_VAR); if (!e) return NULL; e->name = name; return e;
    }
    if (tt == T_LPAREN) { adv(p); Expr *e = parse_expr(p); if (!e) return NULL; if (pk(p) != T_RPAREN) { p_err(p, "缺少 ')'"); return NULL; } adv(p); return e; }
    p_err(p, "表达式中出现意外的记号(位置 %llu)", (unsigned long long)cur(p)->pos);
    return NULL;
}
static Expr *parse_unary(P *p) {
    if (pk(p) == T_MINUS || pk(p) == T_BANG) {
        TokType op = pk(p); adv(p);
        Expr *e = mk(p, E_UN); if (!e) return NULL; e->op = op; e->a = parse_unary(p); if (!e->a) return NULL; return e;
    }
    return parse_primary(p);
}
static Expr *parse_mul(P *p) {
    Expr *l = parse_unary(p); if (!l) return NULL;
    while (pk(p) == T_STAR || pk(p) == T_SLASH || pk(p) == T_PERCENT) {
        TokType op = pk(p); adv(p); Expr *r = parse_unary(p); if (!r) return NULL;
        Expr *e = mk(p, E_BIN); if (!e) return NULL; e->op = op; e->a = l; e->b = r; l = e;
    }
    return l;
}
static Expr *parse_add(P *p) {
    Expr *l = parse_mul(p); if (!l) return NULL;
    while (pk(p) == T_PLUS || pk(p) == T_MINUS) {
        TokType op = pk(p); adv(p); Expr *r = parse_mul(p); if (!r) return NULL;
        Expr *e = mk(p, E_BIN); if (!e) return NULL; e->op = op; e->a = l; e->b = r; l = e;
    }
    return l;
}
static Expr *parse_rel(P *p) {
    Expr *l = parse_add(p); if (!l) return NULL;
    while (pk(p) == T_LT || pk(p) == T_GT || pk(p) == T_LE || pk(p) == T_GE) {
        TokType op = pk(p); adv(p); Expr *r = parse_add(p); if (!r) return NULL;
        Expr *e = mk(p, E_BIN); if (!e) return NULL; e->op = op; e->a = l; e->b = r; l = e;
    }
    return l;
}
static Expr *parse_eq(P *p) {
    Expr *l = parse_rel(p); if (!l) return NULL;
    while (pk(p) == T_EQ || pk(p) == T_NE) {
        TokType op = pk(p); adv(p); Expr *r = parse_rel(p); if (!r) return NULL;
        Expr *e = mk(p, E_BIN); if (!e) return NULL; e->op = op; e->a = l; e->b = r; l = e;
    }
    return l;
}
static Expr *parse_and(P *p) {
    Expr *l = parse_eq(p); if (!l) return NULL;
    while (pk(p) == T_ANDAND) {
        adv(p); Expr *r = parse_eq(p); if (!r) return NULL;
        Expr *e = mk(p, E_BIN); if (!e) return NULL; e->op = T_ANDAND; e->a = l; e->b = r; l = e;
    }
    return l;
}
static Expr *parse_expr(P *p) {
    Expr *l = parse_and(p); if (!l) return NULL;
    while (pk(p) == T_OROR) {
        adv(p); Expr *r = parse_and(p); if (!r) return NULL;
        Expr *e = mk(p, E_BIN); if (!e) return NULL; e->op = T_OROR; e->a = l; e->b = r; l = e;
    }
    return l;
}

static Stmt *parse_block(P *p) {
    if (pk(p) != T_LBRACE) { p_err(p, "期望 '{'"); return NULL; }
    adv(p);
    Stmt *b = mks(p, S_BLOCK); if (!b) return NULL;
    Stmt **list = NULL; int n = 0, cap = 0;
    while (pk(p) != T_RBRACE && pk(p) != T_EOF) {
        Stmt *s = parse_stmt(p); if (!s) return NULL;
        if (n == cap) { int nc = cap ? cap * 2 : 8; Stmt **nl = (Stmt **)ar_alloc(p->a, (size_t)nc * sizeof(Stmt *)); if (!nl) { p_err(p, "内存不足"); return NULL; } if (n) memcpy(nl, list, (size_t)n * sizeof(Stmt *)); list = nl; cap = nc; }
        list[n++] = s;
    }
    if (pk(p) != T_RBRACE) { p_err(p, "缺少 '}'"); return NULL; }
    adv(p); b->list = list; b->n = n; return b;
}

static Stmt *parse_stmt(P *p) {
    TokType tt = pk(p);
    if (tt == T_LBRACE) return parse_block(p);
    if (tt == T_SEMI) { adv(p); return mks(p, S_EMPTY); }
    if (tt == T_KW_INT) {
        adv(p);
        if (pk(p) != T_IDENT) { p_err(p, "变量声明缺少名字"); return NULL; }
        Stmt *s = mks(p, S_DECL); if (!s) return NULL; s->name = cur(p)->name; adv(p);
        if (pk(p) == T_ASSIGN) { adv(p); s->e = parse_expr(p); if (!s->e) return NULL; }
        if (pk(p) != T_SEMI) { p_err(p, "声明缺少 ';'"); return NULL; }
        adv(p); return s;
    }
    if (tt == T_KW_IF) {
        adv(p);
        if (pk(p) != T_LPAREN) { p_err(p, "if 后缺 '('"); return NULL; }
        adv(p); Expr *cond = parse_expr(p); if (!cond) return NULL;
        if (pk(p) != T_RPAREN) { p_err(p, "if 条件缺 ')'"); return NULL; }
        adv(p);
        Stmt *s = mks(p, S_IF); if (!s) return NULL; s->cond = cond;
        s->then = parse_stmt(p); if (!s->then) return NULL;
        if (pk(p) == T_KW_ELSE) { adv(p); s->els = parse_stmt(p); if (!s->els) return NULL; }
        return s;
    }
    if (tt == T_KW_WHILE) {
        adv(p);
        if (pk(p) != T_LPAREN) { p_err(p, "while 后缺 '('"); return NULL; }
        adv(p); Expr *cond = parse_expr(p); if (!cond) return NULL;
        if (pk(p) != T_RPAREN) { p_err(p, "while 条件缺 ')'"); return NULL; }
        adv(p);
        Stmt *s = mks(p, S_WHILE); if (!s) return NULL; s->cond = cond;
        s->then = parse_stmt(p); if (!s->then) return NULL;
        return s;
    }
    if (tt == T_KW_RETURN) {
        adv(p);
        Stmt *s = mks(p, S_RETURN); if (!s) return NULL;
        if (pk(p) != T_SEMI) { s->e = parse_expr(p); if (!s->e) return NULL; }
        if (pk(p) != T_SEMI) { p_err(p, "return 缺 ';'"); return NULL; }
        adv(p); return s;
    }
    /* 表达式语句 或 赋值语句 */
    Expr *e = parse_expr(p); if (!e) return NULL;
    if (pk(p) == T_ASSIGN) {
        if (e->k != E_VAR) { p_err(p, "赋值左边必须是变量"); return NULL; }
        adv(p); Expr *rhs = parse_expr(p); if (!rhs) return NULL;
        if (pk(p) != T_SEMI) { p_err(p, "赋值缺 ';'"); return NULL; }
        adv(p);
        Stmt *s = mks(p, S_ASSIGN); if (!s) return NULL; s->name = e->name; s->e = rhs; return s;
    }
    if (pk(p) != T_SEMI) { p_err(p, "语句缺 ';'"); return NULL; }
    adv(p);
    Stmt *s = mks(p, S_EXPR); if (!s) return NULL; s->e = e; return s;
}

static int const_eval(P *p, Expr *e, int64_t *out) {
    if (e->k == E_NUM) { *out = e->num; return 1; }
    if (e->k == E_UN && e->op == T_MINUS) { int64_t v; if (!const_eval(p, e->a, &v)) return 0; *out = (int64_t)(0ULL - (uint64_t)v); return 1; }
    if (e->k == E_BIN) {
        int64_t a, b;
        if (!const_eval(p, e->a, &a) || !const_eval(p, e->b, &b)) return 0;
        switch (e->op) {
            case T_PLUS: *out = (int64_t)((uint64_t)a + (uint64_t)b); return 1;
            case T_MINUS: *out = (int64_t)((uint64_t)a - (uint64_t)b); return 1;
            case T_STAR: *out = (int64_t)((uint64_t)a * (uint64_t)b); return 1;
            default: return 0;
        }
    }
    return 0;
}

static int push_func(P *p, Func f) {
    if (p->nfuncs == p->fcap) { int nc = p->fcap ? p->fcap * 2 : 8; Func *nf = (Func *)realloc(p->funcs, (size_t)nc * sizeof(Func)); if (!nf) { p_err(p, "内存不足"); return 0; } p->funcs = nf; p->fcap = nc; }
    p->funcs[p->nfuncs++] = f; return 1;
}
static int push_glob(P *p, Global g) {
    if (p->nglobs == p->gcap) { int nc = p->gcap ? p->gcap * 2 : 8; Global *ng = (Global *)realloc(p->globs, (size_t)nc * sizeof(Global)); if (!ng) { p_err(p, "内存不足"); return 0; } p->globs = ng; p->gcap = nc; }
    p->globs[p->nglobs++] = g; return 1;
}
static int push_ext(P *p, Extern e) {
    if (p->nexts == p->ecap) { int nc = p->ecap ? p->ecap * 2 : 8; Extern *ne = (Extern *)realloc(p->exts, (size_t)nc * sizeof(Extern)); if (!ne) { p_err(p, "内存不足"); return 0; } p->exts = ne; p->ecap = nc; }
    p->exts[p->nexts++] = e; return 1;
}

static int parse_externdef(P *p) {
    adv(p); /* extern */
    if (pk(p) != T_KW_INT) { p_err(p, "extern 后缺 int"); return 0; }
    adv(p);
    if (pk(p) != T_IDENT) { p_err(p, "extern 缺函数名"); return 0; }
    Extern e; e.name = cur(p)->name; adv(p);
    if (pk(p) != T_LPAREN) { p_err(p, "extern 缺 '('"); return 0; }
    adv(p);
    int nargs = 0;
    if (pk(p) == T_KW_INT) {
        adv(p);
        if (pk(p) == T_IDENT) adv(p);   /* 参数名可省略（原型） */
        nargs++;
        while (pk(p) == T_COMMA) { adv(p); if (pk(p) != T_KW_INT) { p_err(p, "extern 参数缺 int"); return 0; } adv(p); if (pk(p) == T_IDENT) adv(p); nargs++; }
    }
    if (pk(p) != T_RPAREN) { p_err(p, "extern 缺 ')'"); return 0; }
    adv(p);
    if (pk(p) != T_SEMI) { p_err(p, "extern 缺 ';'"); return 0; }
    adv(p);
    if (nargs != 1) { p_err(p, "extern 宿主函数必须恰好 1 个参数（JCB HOST ABI）"); return 0; }
    return push_ext(p, e);
}

static int parse_funcdef(P *p, char *name) {
    /* 已经吃掉了 int 和 name，当前是 '(' */
    adv(p); /* ( */
    char **params = NULL; int np = 0, pcap = 0;
    if (pk(p) != T_RPAREN) {
        for (;;) {
            if (pk(p) != T_KW_INT) { p_err(p, "参数缺 int"); return 0; }
            adv(p);
            if (pk(p) != T_IDENT) { p_err(p, "参数缺名字"); return 0; }
            if (np == pcap) { int nc = pcap ? pcap * 2 : 4; char **npa = (char **)ar_alloc(p->a, (size_t)nc * sizeof(char *)); if (!npa) { p_err(p, "内存不足"); return 0; } if (np) memcpy(npa, params, (size_t)np * sizeof(char *)); params = npa; pcap = nc; }
            params[np++] = cur(p)->name; adv(p);
            if (pk(p) == T_COMMA) { adv(p); continue; }
            break;
        }
    }
    if (pk(p) != T_RPAREN) { p_err(p, "参数表缺 ')'"); return 0; }
    adv(p);
    if (np > J_MAX_PARAMS) { p_err(p, "参数过多（最多 %d）", J_MAX_PARAMS); return 0; }
    Stmt *body = parse_block(p); if (!body) return 0;
    Func f; f.name = name; f.params = params; f.nparams = np; f.body = body;
    return push_func(p, f);
}

static int parse_globaldef(P *p, char *name) {
    Global g; g.name = name; g.init = 0;
    if (pk(p) == T_ASSIGN) {
        adv(p); Expr *e = parse_expr(p); if (!e) return 0;
        if (!const_eval(p, e, &g.init)) { p_err(p, "全局初值必须是常量表达式"); return 0; }
    }
    if (pk(p) != T_SEMI) { p_err(p, "全局声明缺 ';'"); return 0; }
    adv(p);
    return push_glob(p, g);
}

static int parse_program(P *p) {
    while (pk(p) != T_EOF) {
        TokType tt = pk(p);
        if (tt == T_KW_EXTERN) { if (!parse_externdef(p)) return 0; }
        else if (tt == T_KW_INT) {
            adv(p);
            if (pk(p) != T_IDENT) { p_err(p, "顶层 int 后缺名字"); return 0; }
            char *name = cur(p)->name; adv(p);
            if (pk(p) == T_LPAREN) { if (!parse_funcdef(p, name)) return 0; }
            else { if (!parse_globaldef(p, name)) return 0; }
        } else { p_err(p, "顶层只允许 'int' 声明/函数定义 或 'extern' 原型"); return 0; }
    }
    return 1;
}

/* ===================== 代码生成 ===================== */
typedef struct { char *name; int slot; } LocVar;
typedef struct {
    Arena *a;
    int64_t *cv; int ncv, ccap;          /* 常量池 */
    Func *funcs; int nfuncs;
    Extern *exts; int nexts;
    Global *globs; int nglobs;
    LocVar *locs; int nlocs, loccap;
    int nextslot, nparams;
    int label;
    int err;
    char detail[160];
} Ctx;

static int const_idx(Ctx *c, int64_t v) {
    for (int i = 0; i < c->ncv; i++) if (c->cv[i] == v) return i;
    if (c->ncv == c->ccap) { int nc = c->ccap ? c->ccap * 2 : 16; int64_t *t = (int64_t *)realloc(c->cv, (size_t)nc * sizeof(int64_t)); if (!t) { c->err = 1; return -1; } c->cv = t; c->ccap = nc; }
    c->cv[c->ncv] = v; return c->ncv++;
}
static int add_local(Ctx *c, char *name, int slot) {
    if (c->nlocs == c->loccap) { int nc = c->loccap ? c->loccap * 2 : 16; LocVar *t = (LocVar *)realloc(c->locs, (size_t)nc * sizeof(LocVar)); if (!t) { c->err = 1; return -1; } c->locs = t; c->loccap = nc; }
    c->locs[c->nlocs].name = name; c->locs[c->nlocs].slot = slot; c->nlocs++; return 0;
}
static int find_local(Ctx *c, const char *name) {
    for (int i = c->nlocs - 1; i >= 0; i--) if (strcmp(c->locs[i].name, name) == 0) return c->locs[i].slot;
    return -1;
}
static int find_global(Ctx *c, const char *name) {
    for (int i = 0; i < c->nglobs; i++) if (strcmp(c->globs[i].name, name) == 0) return i;
    return -1;
}
static int find_func(Ctx *c, const char *name) {
    for (int i = 0; i < c->nfuncs; i++) if (strcmp(c->funcs[i].name, name) == 0) return i;
    return -1;
}
static int find_ext(Ctx *c, const char *name) {
    for (int i = 0; i < c->nexts; i++) if (strcmp(c->exts[i].name, name) == 0) return i;
    return -1;
}

static int cg_expr(Ctx *c, SB *o, Expr *e);
static int cg_stmt(Ctx *c, SB *o, Stmt *s);

static const char *binop_str(TokType op) {
    switch (op) {
        case T_PLUS: return "ADD";
        case T_MINUS: return "SUB";
        case T_STAR: return "MUL";
        case T_SLASH: return "DIV";
        case T_PERCENT: return "MOD";
        case T_EQ: return "EQ";
        case T_LT: return "LT";
        case T_GT: return "GT";
        case T_LE: return "LE";
        case T_GE: return "GE";
        default: return NULL;
    }
}

static int cg_expr(Ctx *c, SB *o, Expr *e) {
    if (c->err) return -1;
    switch (e->k) {
        case E_NUM: {
            int k = const_idx(c, e->num); if (k < 0) return -1;
            return sb_printf(o, "PUSH_C %d\n", k);
        }
        case E_VAR: {
            int slot = find_local(c, e->name);
            if (slot >= 0) return sb_printf(o, "LGET %d\n", slot);
            int g = find_global(c, e->name);
            if (g >= 0) return sb_printf(o, "G_GET %d\n", g);
            if (!c->detail[0]) snprintf(c->detail, sizeof c->detail, "未定义变量 '%s'", e->name);
            c->err = 1;
            return -1;
        }
        case E_UN: {
            if (e->op == T_MINUS) {
                int k0 = const_idx(c, 0); if (k0 < 0) return -1;
                if (sb_printf(o, "PUSH_C %d\n", k0)) return -1;
                if (cg_expr(c, o, e->a)) return -1;
                return sb_puts(o, "SUB\n");
            }
            /* !a  =>  (a == 0) */
            if (cg_expr(c, o, e->a)) return -1;
            int k0 = const_idx(c, 0); if (k0 < 0) return -1;
            if (sb_printf(o, "PUSH_C %d\n", k0)) return -1;
            return sb_puts(o, "EQ\n");
        }
        case E_BIN: {
            if (e->op == T_ANDAND || e->op == T_OROR) {
                int lf = c->label++, le = c->label++;
                int k0 = const_idx(c, 0), k1 = const_idx(c, 1);
                if (k0 < 0 || k1 < 0) return -1;
                if (cg_expr(c, o, e->a)) return -1;
                if (e->op == T_ANDAND) {
                    if (sb_printf(o, "JZ L%d\n", lf)) return -1;
                    if (cg_expr(c, o, e->b)) return -1;
                    if (sb_printf(o, "JZ L%d\nPUSH_C %d\nJMP L%d\nL%d:\nPUSH_C %d\nL%d:\n", lf, k1, le, lf, k0, le)) return -1;
                } else {
                    if (sb_printf(o, "JNZ L%d\n", lf)) return -1;
                    if (cg_expr(c, o, e->b)) return -1;
                    if (sb_printf(o, "JNZ L%d\nPUSH_C %d\nJMP L%d\nL%d:\nPUSH_C %d\nL%d:\n", lf, k0, le, lf, k1, le)) return -1;
                }
                return 0;
            }
            if (e->op == T_NE) {
                if (cg_expr(c, o, e->a)) return -1;
                if (cg_expr(c, o, e->b)) return -1;
                int k0 = const_idx(c, 0); if (k0 < 0) return -1;
                if (sb_puts(o, "EQ\n")) return -1;
                if (sb_printf(o, "PUSH_C %d\n", k0)) return -1;
                return sb_puts(o, "EQ\n");
            }
            const char *op = binop_str(e->op);
            if (!op) { c->err = 1; return -1; }
            if (cg_expr(c, o, e->a)) return -1;
            if (cg_expr(c, o, e->b)) return -1;
            return sb_printf(o, "%s\n", op);
        }
        case E_CALL: {
            int ex = find_ext(c, e->name);
            if (ex >= 0) {
                if (e->nargs != 1) { c->err = 1; return -1; }
                if (cg_expr(c, o, e->args[0])) return -1;
                return sb_printf(o, "HOST %d\n", ex);
            }
            int fi = find_func(c, e->name);
            if (fi < 0) { if (!c->detail[0]) snprintf(c->detail, sizeof c->detail, "未定义函数 '%s'", e->name); c->err = 1; return -1; }
            if (e->nargs != c->funcs[fi].nparams) { if (!c->detail[0]) snprintf(c->detail, sizeof c->detail, "函数 '%s' 需要 %d 个参数，给了 %d", e->name, c->funcs[fi].nparams, e->nargs); c->err = 1; return -1; }
            for (int i = 0; i < e->nargs; i++) if (cg_expr(c, o, e->args[i])) return -1;
            return sb_printf(o, "CALL %d\n", fi);
        }
    }
    c->err = 1;
    return -1;
}

static int cg_stmt(Ctx *c, SB *o, Stmt *s) {
    if (c->err) return -1;
    switch (s->k) {
        case S_EMPTY: return 0;
        case S_BLOCK:
            for (int i = 0; i < s->n; i++) if (cg_stmt(c, o, s->list[i])) return -1;
            return 0;
        case S_DECL: {
            int slot = c->nextslot++;
            if (slot >= J_MAX_LOCALS) { c->err = 1; return -1; }
            if (add_local(c, s->name, slot)) return -1;
            if (s->e) {
                if (cg_expr(c, o, s->e)) return -1;
            } else {
                int k0 = const_idx(c, 0); if (k0 < 0) return -1;
                if (sb_printf(o, "PUSH_C %d\n", k0)) return -1;
            }
            return sb_printf(o, "LSET %d\n", slot);
        }
        case S_ASSIGN: {
            if (cg_expr(c, o, s->e)) return -1;
            int slot = find_local(c, s->name);
            if (slot >= 0) return sb_printf(o, "LSET %d\n", slot);
            int g = find_global(c, s->name);
            if (g >= 0) return sb_printf(o, "G_SET %d\n", g);
            if (!c->detail[0]) snprintf(c->detail, sizeof c->detail, "赋值给未定义变量 '%s'", s->name);
            c->err = 1; return -1;
        }
        case S_EXPR:
            if (cg_expr(c, o, s->e)) return -1;
            return sb_puts(o, "DROP\n");
        case S_RETURN:
            if (s->e) { if (cg_expr(c, o, s->e)) return -1; }
            else { int k0 = const_idx(c, 0); if (k0 < 0) return -1; if (sb_printf(o, "PUSH_C %d\n", k0)) return -1; }
            return sb_puts(o, "RET\n");
        case S_IF: {
            int le = c->label++, lend = c->label++;
            if (cg_expr(c, o, s->cond)) return -1;
            if (sb_printf(o, "JZ L%d\n", le)) return -1;
            if (cg_stmt(c, o, s->then)) return -1;
            if (sb_printf(o, "JMP L%d\nL%d:\n", lend, le)) return -1;
            if (s->els) { if (cg_stmt(c, o, s->els)) return -1; }
            return sb_printf(o, "L%d:\n", lend);
        }
        case S_WHILE: {
            int lc = c->label++, lend = c->label++;
            if (sb_printf(o, "L%d:\n", lc)) return -1;
            if (cg_expr(c, o, s->cond)) return -1;
            if (sb_printf(o, "JZ L%d\n", lend)) return -1;
            if (cg_stmt(c, o, s->then)) return -1;
            if (sb_printf(o, "JMP L%d\nL%d:\n", lc, lend)) return -1;
            return 0;
        }
    }
    c->err = 1;
    return -1;
}

static int gen_function_body(Ctx *c, Func *f, SB *body, int *nlocals_out) {
    c->nlocs = 0; c->nextslot = f->nparams; c->nparams = f->nparams; c->err = 0; c->detail[0] = '\0';
    for (int i = 0; i < f->nparams; i++) if (add_local(c, f->params[i], i)) return -1;
    if (cg_stmt(c, body, f->body)) return -1;
    int k0 = const_idx(c, 0); if (k0 < 0) return -1;
    if (sb_printf(body, "PUSH_C %d\nRET\n", k0)) return -1;
    int nl = c->nextslot - f->nparams;
    if (nl < 0 || nl > J_MAX_LOCALS) return -1;
    *nlocals_out = nl;
    return 0;
}

/* ===================== 对外入口 ===================== */
static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }

int j_cc_compile(const char *src, char **out_asm, char **out_err) {
    if (out_asm) *out_asm = NULL;
    if (out_err) *out_err = NULL;
    if (!src) { if (out_err) *out_err = dupstr("空源码"); return -1; }

    Arena a; a.head = NULL;
    TokVec tv; tv.v = NULL; tv.n = 0; tv.cap = 0;
    char lexerr[256]; lexerr[0] = '\0';
    int rc = -1;
    SB *bodies = NULL; int *nlocalsArr = NULL;
    SB out; out.p = NULL; out.n = 0; out.cap = 0;
    P p; memset(&p, 0, sizeof p);        /* 提前零初始化：任何 goto done 都能安全 free */
    Ctx c; memset(&c, 0, sizeof c);

    if (lex_source(src, &a, &tv, lexerr, sizeof lexerr) != 0) {
        if (out_err) *out_err = dupstr(lexerr[0] ? lexerr : "词法错误");
        goto done;
    }

    p.t = tv.v; p.n = tv.n; p.a = &a; p.src = src;
    if (!parse_program(&p)) {
        if (out_err) *out_err = dupstr(p.err[0] ? p.err : "语法错误");
        goto done;
    }
    if (p.nfuncs == 0) { if (out_err) *out_err = dupstr("源码里没有函数"); goto done; }

    int mainidx = -1;
    for (int i = 0; i < p.nfuncs; i++) if (strcmp(p.funcs[i].name, "main") == 0) { mainidx = i; break; }
    if (mainidx < 0) { if (out_err) *out_err = dupstr("缺少 main 函数（编译器入口）"); goto done; }
    if (p.funcs[mainidx].nparams != 0) { if (out_err) *out_err = dupstr("main 不能带参数（入口以无参方式调用）"); goto done; }

    c.a = &a; c.funcs = p.funcs; c.nfuncs = p.nfuncs; c.exts = p.exts; c.nexts = p.nexts;
    c.globs = p.globs; c.nglobs = p.nglobs; c.label = 0;

    bodies = (SB *)calloc((size_t)p.nfuncs, sizeof(SB));
    nlocalsArr = (int *)calloc((size_t)p.nfuncs, sizeof(int));
    if (!bodies || !nlocalsArr) { if (out_err) *out_err = dupstr("内存不足"); goto done; }

    for (int i = 0; i < p.nfuncs; i++) {
        if (gen_function_body(&c, &p.funcs[i], &bodies[i], &nlocalsArr[i]) != 0 || c.err) {
            char msg[256];
            if (c.detail[0]) snprintf(msg, sizeof msg, "函数 '%s': %s", p.funcs[i].name, c.detail);
            else snprintf(msg, sizeof msg, "编译函数 '%s' 失败（可能：参数个数不符、局部变量过多）", p.funcs[i].name);
            if (out_err) *out_err = dupstr(msg);
            goto done;
        }
    }

    /* 组装最终汇编文本：常量 -> 全局 -> 各函数 */
    for (int i = 0; i < c.ncv; i++) if (sb_printf(&out, ".const %lld\n", (long long)c.cv[i])) goto done;
    for (int i = 0; i < p.nglobs; i++) if (sb_printf(&out, ".global %lld\n", (long long)p.globs[i].init)) goto done;
    for (int i = 0; i < p.nfuncs; i++) {
        if (sb_printf(&out, ".function %d %d %s\n", p.funcs[i].nparams, nlocalsArr[i], i == mainidx ? "main" : "-")) goto done;
        if (sb_putn(&out, bodies[i].p, bodies[i].n)) goto done;
        if (sb_puts(&out, ".end\n")) goto done;
    }
    if (out_asm) { *out_asm = out.p; out.p = NULL; }
    rc = 0;

done:
    if (bodies) { for (int i = 0; i < p.nfuncs; i++) sb_free(&bodies[i]); free(bodies); }
    free(nlocalsArr);
    sb_free(&out);
    free(c.cv);
    free(c.locs);
    free(p.funcs);
    free(p.globs);
    free(p.exts);
    free(tv.v);
    ar_free(&a);
    return rc;
}
