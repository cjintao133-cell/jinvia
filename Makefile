# Jinvia — 纯 C，零第三方依赖
CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11
INC     = -Iinclude
SRC     = src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jcb_disasm.c src/jcb_cc.c src/jinvia.c src/main.c

all: jinvia

jinvia: $(SRC) include/jinvia/cap.h include/jinvia/jcb.h
	$(CC) $(CFLAGS) $(INC) $(SRC) -o jinvia

test:
	bash test-c.sh

portability:
	bash portability.sh

lib:
	bash lib.sh

cov:
	bash cov.sh 80

soak:
	bash fuzz-soak.sh 300

bench:
	@mkdir -p build
	$(CC) $(CFLAGS) $(INC) src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c tests/bench.c -o build/bench
	./build/bench

clean:
	rm -f jinvia
	rm -rf build

.PHONY: all test lib portability cov soak bench clean
