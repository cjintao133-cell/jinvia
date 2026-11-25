/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JINVIA_H
#define JINVIA_H
/* Jinvia 高层嵌入 API —— 把 Jinvia 当库嵌进你自己的程序。
 * 纯 C11，零第三方依赖。典型用法：
 *   JinviaRuntime *rt = jinvia_new();
 *   jinvia_register_host(rt, "read:temp", my_read_temp, NULL);
 *   jinvia_register_host(rt, "call:alarm", my_alarm, NULL);
 *   jinvia_set_limits(rt, 1000000, 4096, 256);
 *   if (jinvia_load(rt, conf_text, jcb_bytes, jcb_len) == JINVIA_OK) {
 *       int64_t res; jinvia_call(rt, &res);
 *   }
 *   jinvia_free(rt);
 *
 * 安全模型：能力只有在它的 needs 全部被宿主"注册"后才会被加载；未注册的宿主能力，
 * 能力要了也白搭（默认拒绝）。能力字节码里的 HOST idx = 宿主注册顺序。 */
#include <stdint.h>
#include <stddef.h>
#include "jinvia/jcb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JinviaRuntime JinviaRuntime;
/* 宿主函数：收到一个 i64 参数，返回一个 i64 结果。ud 为注册时传入的用户指针。 */
typedef int64_t (*JinviaHostFn)(void *ud, int64_t arg);

/* 返回码（复用 JErr 负数，另加高层错误） */
#define JINVIA_OK           J_OK
#define JINVIA_ERR_NULL     (-101)
#define JINVIA_ERR_NEEDS    (-102)  /* 能力申请的 needs 宿主没注册（默认拒绝） */
#define JINVIA_ERR_HOSTIDX  (-103)  /* 字节码 HOST idx 超出已注册宿主 */
#define JINVIA_ERR_NOLOAD   (-104)  /* 尚未 load 就 call */

JinviaRuntime *jinvia_new(void);
void           jinvia_free(JinviaRuntime *rt);

/* 注册一个宿主能力，返回分配到的 host idx（从 0 递增）；失败返回负。 */
int  jinvia_register_host(JinviaRuntime *rt, const char *name, JinviaHostFn fn, void *ud);
/* 设置资源预算（0 表示用默认/钳制到上限）。 */
int  jinvia_set_limits(JinviaRuntime *rt, uint64_t max_steps, uint32_t max_stack, uint32_t max_frames);
/* 设墙钟时间预算(毫秒,0=不限)，防单次调用卡死宿主。 */
int  jinvia_set_time_budget(JinviaRuntime *rt, uint64_t ms);

/* 加载一个能力包（conf 文本 + jcb 字���）。成功后可 jinvia_call。重复 load 会替换旧的。 */
int  jinvia_load(JinviaRuntime *rt, const char *conf_text, const uint8_t *jcb, size_t jcb_len);
/* 调用已加载能力的入口（无参），结果写入 *result。 */
int  jinvia_call(JinviaRuntime *rt, int64_t *result);
/* 带参数调用入口：args/nargs 逐字节为 i64（对应能力的 params 声明顺序）。
 * nargs 必须等于入口函数的形参个数，否则返回 J_ERR_FMT。 */
int  jinvia_call_args(JinviaRuntime *rt, const int64_t *args, uint32_t nargs, int64_t *result);
/* 查询已加载能力入口函数的形参个数；未加载返回 -1。 */
int  jinvia_entry_nparams(JinviaRuntime *rt);
/* 已加载能力的线性内存：查询字节大小 / 在调用前写入若干字节（供字符串参数注入）。
 * 越界或未加载返回 -1。 */
uint32_t jinvia_mem_size(JinviaRuntime *rt);
int      jinvia_mem_write(JinviaRuntime *rt, uint32_t off, const void *src, uint32_t len);

/* 只读查询（便于宿主/测试） */
const char *jinvia_cap_name(JinviaRuntime *rt);   /* 已加载能力名，未加载返回 NULL */
int         jinvia_host_count(JinviaRuntime *rt);

#ifdef __cplusplus
}
#endif
#endif
