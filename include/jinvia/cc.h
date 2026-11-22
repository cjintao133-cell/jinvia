/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JINVIA_CC_H
#define JINVIA_CC_H
#include <stddef.h>
/* Jinvia C 子集编译器 —— 把一小段类 C 源码编译成 JCB 汇编文本(.jcbasm)，
 * 再由 j_asm() 汇编成字节码。给作者一个"写 C、不写汇编"的入口。
 * 纯 C11，零第三方依赖。成功返回 0，*out_asm 为 malloc 的汇编文本（调用方 free）；
 * 失败返回负码，*out_err（若非 NULL）为 malloc 的错误说明文本。 */
int j_cc_compile(const char *src, char **out_asm, char **out_err);
#endif
