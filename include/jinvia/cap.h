/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JINVIA_CAP_H
#define JINVIA_CAP_H

/* Jinvia capability.conf parser — pure C11, no third-party deps, no malloc.
 * 格式：一行一条 "key: value"；# 开头为注释；空行忽略。
 * needs 用逗号分隔。成功返回 0，NULL 或缺 name 返回 -1。 */

#define SR_NAME_MAX  128
#define SR_DOES_MAX  512
#define SR_ENTRY_MAX 256
#define SR_NEEDS_MAX 16
#define SR_NEED_LEN  64

typedef struct {
    char name[SR_NAME_MAX];
    char does[SR_DOES_MAX];
    char entry[SR_ENTRY_MAX];
    char needs[SR_NEEDS_MAX][SR_NEED_LEN];
    int  needs_count;
} SrCap;

int sr_cap_parse(const char *text, SrCap *out);

/* 铁律1「默认拒绝」的可测实现：cap 的每一个 need 都必须在 grants 里，
 * 全满足返回 1，否则返回 0（有任何一个没授权就整体不放行）。 */
int sr_cap_needs_satisfied(const SrCap *cap, const char *const *grants, int ngrants);

#endif /* JINVIA_CAP_H */
