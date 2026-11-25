#!/usr/bin/env bash
# Jinvia C 门禁：normal(-Werror) + ASan+UBSan + Valgrind，串行。四道门全绿才叫"能用"。
set -uo pipefail
cd "$(dirname "$0")"
INC="-Iinclude"
FLAGS="-O2 -Wall -Wextra -Werror -std=c11"
OUT=build/test; mkdir -p "$OUT"
CC=${CC:-gcc}
fail=0
run_suite () {
  local name="$1"; shift; local srcs="$*"
  echo "----- suite: $name -----"
  echo " [normal]";   if $CC $FLAGS $INC $srcs -o "$OUT/$name"     && "$OUT/$name";     then echo " [OK] normal"; else echo " [FAIL] normal"; fail=1; fi
  echo " [ASan+UBSan]"; if $CC -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -std=c11 $INC $srcs -o "$OUT/$name-san" && "$OUT/$name-san"; then echo " [OK] asan+ubsan"; else echo " [FAIL] asan+ubsan"; fail=1; fi
  echo " [Valgrind]"; if command -v valgrind >/dev/null 2>&1; then
      if $CC $FLAGS $INC $srcs -o "$OUT/$name-vg" && valgrind -q --leak-check=full --error-exitcode=99 "$OUT/$name-vg"; then echo " [OK] valgrind"; else echo " [FAIL] valgrind"; fail=1; fi
    else echo " [SKIP] valgrind 未安装"; fi
}
run_suite cap  src/cap.c tests/test_cap.c
run_suite jcb  src/jcb_read.c tests/test_jcb.c
run_suite decode src/jcb_read.c src/jcb_decode.c tests/test_decode.c
run_suite vm     src/jcb_read.c src/jcb_decode.c src/jcb_vm.c tests/test_vm.c
run_suite asm    src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_asm.c
run_suite edge   src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_edge.c
run_suite limits src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_limits.c
run_suite memory src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_memory.c
run_suite bits   src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_bits.c
run_suite fillcopy src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_fillcopy.c
run_suite globals src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_globals.c
run_suite compare src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/test_compare.c
run_suite embed  src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jinvia.c tests/test_embed.c
run_suite verify src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jcb_disasm.c tests/test_verify.c
run_suite float  src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c tests/test_float.c
run_suite cc     src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jcb_cc.c tests/test_cc.c
run_suite mcp    src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jinvia.c src/mcp.c tests/test_mcp.c

# CLI 集成：编译 jinvia，跑示例能力，校验退出码与输出
echo "----- cli integration -----"
if $CC $FLAGS $INC src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jcb_disasm.c src/jcb_cc.c src/mcp.c src/jinvia.c src/main.c -o "$OUT/jinvia"; then
  out=$("$OUT/jinvia" run examples/thermostat-guard 2>&1); rc=$?
  if [ $rc -eq 0 ] && echo "$out" | grep -q "报警" && echo "$out" | grep -q "结果 = 1"; then echo " [OK] thermostat-guard"; else echo " [FAIL] thermostat-guard"; fail=1; fi
  "$OUT/jinvia" run examples/evil >/dev/null 2>&1; [ $? -eq 3 ] && echo " [OK] evil denied(3)" || { echo " [FAIL] evil not denied"; fail=1; }
  "$OUT/jinvia" run examples/sneaky >/dev/null 2>&1; [ $? -eq 3 ] && echo " [OK] undeclared-host bypass denied(3)" || { echo " [FAIL] undeclared-host bypass NOT denied"; fail=1; }
  out=$("$OUT/jinvia" run --trace examples/array-sum 2>&1); rc=$?
  if [ $rc -eq 0 ] && echo "$out" | grep -q "结果 = 60"; then echo " [OK] array-sum"; else echo " [FAIL] array-sum"; fail=1; fi
  out=$("$OUT/jinvia" run --trace examples/door-guard 2>&1); rc=$?
  if [ $rc -eq 0 ] && echo "$out" | grep -q "开门" && echo "$out" | grep -q "结果 = 1"; then echo " [OK] door-guard"; else echo " [FAIL] door-guard"; fail=1; fi
  # C 子集编译器端到端：fib.c -> .jcbasm -> .jcb -> run
  mkdir -p build/ccdemo
  if "$OUT/jinvia" cc examples/cc/fib.c build/ccdemo/main.jcbasm >/dev/null 2>&1 \
     && "$OUT/jinvia" asm build/ccdemo/main.jcbasm build/ccdemo/main.jcb >/dev/null 2>&1 \
     && printf 'name: main\ndoes: fib\nneeds:\nentry: main.jcb\n' > build/ccdemo/capability.conf \
     && out=$("$OUT/jinvia" run build/ccdemo 2>&1) && echo "$out" | grep -q "结果 = 6765"; then echo " [OK] cc e2e fib=6765"; else echo " [FAIL] cc e2e"; fail=1; fi
  # MCP stdio e2e：喂 initialize/tools-list/tools-call，校验 result=55
  mout=$(printf '%s\n' \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' \
    '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"sum-1-to-10"}}' | "$OUT/jinvia" mcp examples 2>/dev/null)
  if echo "$mout" | grep -q "result = 55" && echo "$mout" | grep -q "thermostat-guard"; then echo " [OK] mcp e2e result=55"; else echo " [FAIL] mcp e2e"; fail=1; fi
else echo " [FAIL] cli build"; fail=1; fi


# 沙箱对抗：7 个恶意能力必须全部被拦（加载期 exit3 / 运行期 exit4），绝不存活或崩溃
echo "----- sandbox showdown -----"
sd=0
declare -A _want=( [a1-unauthorized]=3 [a2-host-escape]=3 [a3-divzero]=4 [a4-sigfpe]=4 [a5-oob-read]=4 [a6-infinite]=4 [a7-recursion]=4 )
for _d in examples/sandbox-showdown/a*; do
  _bn=$(basename "$_d")
  "$OUT/jinvia" asm "$_d/main.jcbasm" "$_d/main.jcb" >/dev/null 2>&1
  "$OUT/jinvia" run "$_d" >/dev/null 2>&1; _rc=$?
  if [ "$_rc" = "${_want[$_bn]}" ]; then echo " [OK] $_bn blocked($_rc)"; else echo " [FAIL] $_bn rc=$_rc want ${_want[$_bn]}"; sd=1; fi
done
[ $sd -eq 0 ] && echo " [OK] sandbox showdown 7/7" || fail=1

# fuzz：ASan+UBSan 下随机+变异输入，验证不崩不漏
echo "----- fuzz (ASan+UBSan) -----"
if $CC -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -std=c11 $INC src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c tests/fuzz_jcb.c -o "$OUT/fuzz" && "$OUT/fuzz" 250000; then echo " [OK] fuzz"; else echo " [FAIL] fuzz"; fail=1; fi

echo "== RESULT =="
if [ "$fail" -eq 0 ]; then echo "C-GATE GREEN"; else echo "C-GATE RED"; fi
exit "$fail"
