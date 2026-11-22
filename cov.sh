#!/usr/bin/env bash
# 引擎行覆盖率门禁：单测全跑 + fuzz，gcov 统计 src/*.c 的行覆盖，低于阈值即红。
set -uo pipefail
cd "$(dirname "$0")"
THRESH=${1:-80}
ENG="src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jcb_disasm.c src/jinvia.c"
SUITES="test_cap test_jcb test_decode test_vm test_asm test_edge test_limits test_memory test_bits test_fillcopy test_globals test_compare test_embed test_verify test_float"
COV=build/cov; rm -rf "$COV"; mkdir -p "$COV/eng"
rm -f *.gcov
for s in $ENG; do
  b=$(basename "$s" .c)
  gcc -O0 --coverage -Iinclude -c "$s" -o "$COV/eng/$b.o" || exit 1
done
ENGOBJS=""; for s in $ENG; do ENGOBJS="$ENGOBJS $COV/eng/$(basename "$s" .c).o"; done
for t in $SUITES; do
  gcc -O0 -Iinclude -c "tests/$t.c" -o "$COV/$t.o" 2>/dev/null || continue
  gcc --coverage $ENGOBJS "$COV/$t.o" -lm -o "$COV/bin_$t" 2>/dev/null || continue
  "$COV/bin_$t" >/dev/null 2>&1
done
# fuzz 也计入
gcc -O0 -Iinclude -c tests/fuzz_jcb.c -o "$COV/fuzz.o" 2>/dev/null && \
  gcc --coverage $ENGOBJS "$COV/fuzz.o" -o "$COV/bin_fuzz" 2>/dev/null && "$COV/bin_fuzz" 300000 >/dev/null 2>&1
gcov -o "$COV/eng" $(for s in $ENG; do basename "$s"; done) >/dev/null 2>&1
python3 - "$THRESH" <<'PY'
import glob,re,sys
thr=float(sys.argv[1]); ENG=['cap.c','jcb_read.c','jcb_decode.c','jcb_vm.c','jcb_asm.c','jcb_verify.c','jcb_disasm.c','jinvia.c']
data={}
for gf in glob.glob('*.gcov'):
    src=None
    for line in open(gf):
        m=re.match(r'\s*([^:]+):\s*(\d+):(.*)',line)
        if not m: continue
        cnt,ln,txt=m.group(1).strip(),int(m.group(2)),m.group(3)
        if txt.startswith('Source:'): src=txt.split('Source:',1)[1].strip(); continue
        if src is None or cnt=='-': continue
        d=data.setdefault(src,{}); d[ln]=d.get(ln,False) or cnt.replace('#','').replace('=','').isdigit()
ta=ca=0
print("==== 引擎行覆盖率（单测+fuzz 并集）====")
for e in ENG:
    key=next((k for k in data if k.endswith(e)),None)
    if not key: print(f"  {e}: 无数据"); continue
    d=data[key]; tot=len(d); cov=sum(1 for v in d.values() if v); ta+=tot; ca+=cov
    print(f"  {e:<14} {cov}/{tot}  {100.0*cov/tot:5.1f}%")
pct=100.0*ca/ta
print(f"  合计 {ca}/{ta}  {pct:.1f}%   阈值 {thr:.0f}%")
print("COV-GATE GREEN" if pct>=thr else "COV-GATE RED")
sys.exit(0 if pct>=thr else 1)
PY
