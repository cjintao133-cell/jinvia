# Governance

Jinvia is currently maintained by a single maintainer.

## Decision making
- Lazy consensus: proposals (issues/PRs) are accepted if no maintainer objection and CI passes.
- Security-relevant changes require passing the full gate (normal + ASan/UBSan + Valgrind + fuzz) and, ideally, a second pair of eyes.

## Roles
- Maintainer: reviews and merges, cuts releases, handles security reports (see SECURITY.md).
- Contributors: submit PRs per CONTRIBUTING.md (DCO sign-off).

## Continuity
Source is public and reproducible from a single C compiler; see ROADMAP.md for direction.
