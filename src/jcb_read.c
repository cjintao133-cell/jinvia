/* SPDX-License-Identifier: Apache-2.0 */
#include "jinvia/jcb.h"
#include <string.h>
#include <stdlib.h>

typedef struct { const uint8_t *p, *end; } JR;

static int ru8(JR *r, uint8_t *o) { if (r->p >= r->end) return J_ERR_EOF; *o = *r->p++; return J_OK; }
static int ru16(JR *r, uint16_t *o) {
    if ((size_t)(r->end - r->p) < 2) return J_ERR_EOF;
    *o = (uint16_t)((uint16_t)r->p[0] | ((uint16_t)r->p[1] << 8)); r->p += 2; return J_OK;
}
static int ru32(JR *r, uint32_t *o) {
    if ((size_t)(r->end - r->p) < 4) return J_ERR_EOF;
    *o = (uint32_t)r->p[0] | ((uint32_t)r->p[1] << 8) | ((uint32_t)r->p[2] << 16) | ((uint32_t)r->p[3] << 24);
    r->p += 4; return J_OK;
}
static int ru64(JR *r, uint64_t *o) {
    if ((size_t)(r->end - r->p) < 8) return J_ERR_EOF;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)r->p[i] << (8 * i);
    r->p += 8; *o = v; return J_OK;
}
static int rbytes(JR *r, uint8_t *dst, size_t n) {
    if ((size_t)(r->end - r->p) < n) return J_ERR_EOF;
    memcpy(dst, r->p, n); r->p += n; return J_OK;
}

static int parse_consts(JR *sr, JModule *m) {
    uint32_t n; int e = ru32(sr, &n); if (e) return e;
    if (n > J_MAX_CONSTS) return J_ERR_LIMIT;
    if (n == 0) return J_OK;
    free(m->consts);
    m->consts = (int64_t *)calloc(n, sizeof(int64_t)); if (!m->consts) return J_ERR_OOM;
    m->nconsts = n;
    for (uint32_t i = 0; i < n; i++) {
        uint64_t u; e = ru64(sr, &u); if (e) return e;
        m->consts[i] = (int64_t)u;
    }
    return J_OK;
}

static int parse_funcs(JR *sr, JModule *m) {
    uint32_t n; int e = ru32(sr, &n); if (e) return e;
    if (n > J_MAX_FUNCS) return J_ERR_LIMIT;
    if (n == 0) return J_OK;
    free(m->funcs);
    m->funcs = (JFunc *)calloc(n, sizeof(JFunc)); if (!m->funcs) return J_ERR_OOM;
    m->nfuncs = n;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t np, nl, cl;
        e = ru32(sr, &np); if (e) return e;
        e = ru32(sr, &nl); if (e) return e;
        e = ru32(sr, &cl); if (e) return e;
        if (cl > J_MAX_CODE || np > J_MAX_PARAMS || nl > J_MAX_LOCALS) return J_ERR_LIMIT;
        m->funcs[i].nparams = np; m->funcs[i].nlocals = nl; m->funcs[i].code_len = cl;
        if (cl > 0) {
            m->funcs[i].code = (uint8_t *)malloc(cl);
            if (!m->funcs[i].code) return J_ERR_OOM;
            e = rbytes(sr, m->funcs[i].code, cl); if (e) return e;
        }
    }
    return J_OK;
}

static int parse_exports(JR *sr, JModule *m) {
    uint32_t n; int e = ru32(sr, &n); if (e) return e;
    if (n > J_MAX_EXPORTS) return J_ERR_LIMIT;
    if (n == 0) return J_OK;
    free(m->export_names); free(m->export_idx);
    m->export_names = calloc(n, J_MAX_NAME);
    m->export_idx = (uint32_t *)calloc(n, sizeof(uint32_t));
    if (!m->export_names || !m->export_idx) return J_ERR_OOM;
    m->nexports = n;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t nl; e = ru16(sr, &nl); if (e) return e;
        if (nl >= J_MAX_NAME) return J_ERR_LIMIT;
        e = rbytes(sr, (uint8_t *)m->export_names[i], nl); if (e) return e;
        m->export_names[i][nl] = '\0';
        uint32_t idx; e = ru32(sr, &idx); if (e) return e;
        m->export_idx[i] = idx;
    }
    return J_OK;
}

static int parse_globals(JR *sr, JModule *m) {
    uint32_t n; int e = ru32(sr, &n); if (e) return e;
    if (n > J_MAX_GLOBALS) return J_ERR_LIMIT;
    if (n == 0) return J_OK;
    free(m->globals);
    m->globals = (int64_t *)calloc(n, sizeof(int64_t)); if (!m->globals) return J_ERR_OOM;
    m->nglobals = n;
    for (uint32_t i = 0; i < n; i++) { uint64_t u; e = ru64(sr, &u); if (e) return e; m->globals[i] = (int64_t)u; }
    return J_OK;
}

static int parse_memory(JR *sr, JModule *m) {
    uint32_t pages; int e = ru32(sr, &pages); if (e) return e;
    if (pages > J_MAX_MEM_PAGES) return J_ERR_LIMIT;
    m->mem_pages = pages;
    return J_OK;
}

int j_parse(const uint8_t *buf, size_t len, JModule *out) {
    if (!buf || !out) return J_ERR_EOF;
    memset(out, 0, sizeof(*out));
    JR r = { buf, buf + len };
    int e;

    uint8_t magic[3];
    e = rbytes(&r, magic, 3); if (e) goto fail;
    if (!(magic[0]=='J' && magic[1]=='C' && magic[2]=='B')) { e = J_ERR_MAGIC; goto fail; }
    uint8_t ver; e = ru8(&r, &ver); if (e) goto fail;
    if (ver != 0x01) { e = J_ERR_VERSION; goto fail; }

    uint8_t seen[256]; memset(seen, 0, sizeof(seen));
    while (r.p < r.end) {
        uint8_t id; e = ru8(&r, &id); if (e) goto fail;
        if (seen[id]) { e = J_ERR_FMT; goto fail; }
        seen[id] = 1;
        uint32_t size; e = ru32(&r, &size); if (e) goto fail;
        if ((size_t)(r.end - r.p) < size) { e = J_ERR_SECTION; goto fail; }
        JR sr = { r.p, r.p + size };
        switch (id) {
            case 1: e = parse_consts(&sr, out);  if (e) goto fail; break;
            case 2: e = parse_funcs(&sr, out);   if (e) goto fail; break;
            case 7: e = parse_exports(&sr, out); if (e) goto fail; break;
            case 3: e = parse_memory(&sr, out);  if (e) goto fail; break;
            case 4: e = parse_globals(&sr, out); if (e) goto fail; break;
            default: break;
        }
        r.p += size;
    }
    return J_OK;
fail:
    j_free(out);
    return e;
}

void j_free(JModule *m) {
    if (!m) return;
    free(m->consts); m->consts = NULL;
    if (m->funcs) { for (uint32_t i = 0; i < m->nfuncs; i++) free(m->funcs[i].code); free(m->funcs); m->funcs = NULL; }
    free(m->globals); m->globals = NULL;
    free(m->export_names); m->export_names = NULL;
    free(m->export_idx); m->export_idx = NULL;
    m->nconsts = m->nfuncs = m->nexports = m->nglobals = 0;
}
