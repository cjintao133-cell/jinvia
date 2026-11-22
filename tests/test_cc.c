#include "jinvia/cc.h"
#include "jinvia/jcb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
static void ck(int c, const char *m, int ln) { if (!c) { printf("  FAIL line %d: %s\n", ln, m); fails++; } }

static uint32_t entry_of(JModule *m, const char *name) {
    for (uint32_t i = 0; i < m->nexports; i++) if (strcmp(m->export_names[i], name) == 0) return m->export_idx[i];
    return 0;
}
static int64_t g_host_arg;
static int64_t h_twice(void *ud, uint32_t idx, int64_t a) { (void)ud; (void)idx; g_host_arg = a; return a * 2; }

/* C 源码 -> 汇编 -> 字节码 -> 校验 -> 运行 main（0 参），结果写入 *out。返回 0 成功。 */
static int compile_run(const char *src, int64_t *out) {
    char *asmtxt = NULL, *err = NULL;
    if (j_cc_compile(src, &asmtxt, &err) != 0) { if (err) printf("    [cc] %s\n", err); free(err); return -1; }
    free(err);
    uint8_t *buf = NULL; size_t len = 0;
    int rc = j_asm(asmtxt, &buf, &len);
    free(asmtxt);
    if (rc != J_OK) return -2;
    JModule m;
    if (j_parse(buf, len, &m) != J_OK) { free(buf); return -3; }
    free(buf);
    if ((rc = j_verify(&m)) != J_OK) { j_free(&m); return -4; }
    JVM *vm = j_vm_new(&m);
    if (!vm) { j_free(&m); return -5; }
    j_vm_set_host(vm, h_twice, NULL);
    int64_t r = 0;
    rc = j_vm_call(vm, entry_of(&m, "main"), NULL, 0, &r);
    j_vm_free(vm); j_free(&m);
    if (rc != J_OK) return -6;
    *out = r;
    return 0;
}

static void expect(const char *label, const char *src, int64_t want, int ln) {
    int64_t got = 0;
    int rc = compile_run(src, &got);
    int ok = (rc == 0 && got == want);
    if (!ok) printf("    [%s] rc=%d got=%lld want=%lld\n", label, rc, (long long)got, (long long)want);
    ck(ok, label, ln);
}
/* 期望编译失败（返回负，且给出错误信息不崩） */
static void expect_reject(const char *label, const char *src, int ln) {
    char *asmtxt = NULL, *err = NULL;
    int rc = j_cc_compile(src, &asmtxt, &err);
    ck(rc != 0, label, ln);
    ck(asmtxt == NULL, "no asm on reject", ln);
    ck(err != NULL && err[0] != '\0', "error message present", ln);
    free(asmtxt); free(err);
}

int main(void) {
    /* --- 正确性：算术与优先级 --- */
    expect("arith_prec", "int main(){ return 2+3*4; }", 14, __LINE__);
    expect("paren", "int main(){ return (2+3)*4; }", 20, __LINE__);
    expect("sub_div_mod", "int main(){ return 100-10/2-7%4; }", 92, __LINE__);
    expect("unary_minus", "int main(){ return 5 + -3; }", 2, __LINE__);

    /* --- 变量 + 循环 --- */
    expect("sum_1_10", "int main(){ int s=0; int i=0; while(i<11){ s=s+i; i=i+1; } return s; }", 55, __LINE__);
    expect("count_down", "int main(){ int i=10; int n=0; while(i>0){ n=n+1; i=i-1; } return n; }", 10, __LINE__);

    /* --- 分支 --- */
    expect("if_true", "int main(){ int x=7; if(x>5) return 1; else return 0; }", 1, __LINE__);
    expect("if_false", "int main(){ int x=2; if(x>5) return 1; else return 0; }", 0, __LINE__);

    /* --- 比较与逻辑 --- */
    expect("cmp_mix", "int main(){ return (3<=3)+(4!=4)+(2<1); }", 1, __LINE__);
    expect("and_or", "int main(){ int a=1; int b=0; return (a&&!b)||0; }", 1, __LINE__);
    expect("short_circuit", "int main(){ int a=0; return a && (1/0); }", 0, __LINE__);

    /* --- 函数 + 递归 --- */
    expect("fib", "int fib(int n){ if(n<2) return n; return fib(n-1)+fib(n-2); } int main(){ return fib(15); }", 610, __LINE__);
    expect("fact", "int fact(int n){ if(n<=1) return 1; return n*fact(n-1); } int main(){ return fact(6); }", 720, __LINE__);
    expect("gcd", "int gcd(int a,int b){ while(b!=0){ int t=b; b=a%b; a=t; } return a; } int main(){ return gcd(48,36); }", 12, __LINE__);
    expect("nested_call", "int add(int a,int b){ return a+b; } int main(){ return add(add(1,2),add(3,4)); }", 10, __LINE__);

    /* --- 全局变量 --- */
    expect("global", "int g; int main(){ g=41; return g+1; }", 42, __LINE__);
    expect("global_init", "int g=100; int main(){ return g/4; }", 25, __LINE__);

    /* --- extern 宿主调用 --- */
    expect("extern_host", "extern int twice(int); int main(){ return twice(21); }", 42, __LINE__);
    ck(g_host_arg == 21, "extern received arg", __LINE__);

    /* --- 可复现的真实小程序：素数计数（< 50 的素数个数） --- */
    expect("primes_under_50",
           "int isprime(int n){ int d=2; if(n<2) return 0; while(d*d<=n){ if(n%d==0) return 0; d=d+1; } return 1; }"
           "int main(){ int c=0; int i=2; while(i<50){ c=c+isprime(i); i=i+1; } return c; }",
           15, __LINE__);

    /* --- 编译期拒绝（不崩、有错误信息） --- */
    expect_reject("reject_syntax", "int main(){ return 1+; }", __LINE__);
    expect_reject("reject_undef_var", "int main(){ return x; }", __LINE__);
    expect_reject("reject_main_params", "int main(int a){ return a; }", __LINE__);
    expect_reject("reject_no_main", "int f(){ return 1; }", __LINE__);
    expect_reject("reject_undef_func", "int main(){ return foo(1); }", __LINE__);
    expect_reject("reject_missing_semi", "int main(){ return 1 }", __LINE__);
    expect_reject("reject_extern_arity", "extern int f(int,int); int main(){ return 0; }", __LINE__);

    printf(fails ? "test_cc: %d FAIL\n" : "test_cc: ALL PASS\n", fails);
    return fails ? 1 : 0;
}
