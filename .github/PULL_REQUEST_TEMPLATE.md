## What & why

<!-- What does this change do, and why? -->

## Checklist

- [ ] `make && bash test-c.sh` is green (normal + ASan/UBSan + Valgrind + fuzz)
- [ ] `bash cov.sh 80` still passes
- [ ] `bash portability.sh` is clean
- [ ] New parsing/memory code has tests (and a fuzz input if relevant)
- [ ] C11, no new third-party dependencies
- [ ] Docs updated if behavior changed
