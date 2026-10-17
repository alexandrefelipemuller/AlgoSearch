# Repository Guidelines

AlgoSearch is a C++20 program-synthesis framework: beam search over a small imperative
AST language ("Mini-C"), with no machine learning. `README.md` explains *how* it works;
`FINDINGS.md` records *why* it does or doesn't. Read `FINDINGS.md` before changing
`src/search/` or `src/language/`.

## Project Structure & Module Organization

- `src/ast/` — `Expr`/`Stmt` node definitions and `size()`.
- `src/language/` — `PrettyPrinter`; grammar changes also touch `src/search/ProgramGenerator.cpp`.
- `src/interpreter/` — tree-walking `Interpreter` with a step limit.
- `src/problems/` — one `Problem` subclass per benchmark problem, plus `Problem.hpp` (the
  interface the search programs against) and `ArrayInstances.cpp` (shared instance helpers).
- `src/search/` — `BeamSearch`, `Benchmark` (independent verification), `Persistence`, `Logger`.
- `src/core/` — `Random` and shared types. `src/main.cpp` — CLI and problem registry.
- `tests/`, `scripts/` (battery runners), `data/` (experiment output, committed).

## Build, Test, and Development Commands

```sh
make                 # build ./algosearch (plain Makefile, no CMake)
make test            # build and run ./algosearch_tests
make clean           # remove build/ and both binaries
./algosearch --help  # full flag list (beam width, candidates, diversity, grammar bias)
```

Example search: `./algosearch --problem gcd --max-n 20 --beam-width 500 --candidates 1500
--max-program-size 150 --seed 1000 --benchmark --benchmark-seeds 5`.
Sweep all problems with `scripts/run_full_battery_modest.sh`.

## Coding Style & Naming Conventions

- 4-space indent, ~120-column lines, `#pragma once`, includes grouped system-then-project.
- `PascalCase` for types and files, `camelCase` for methods and variables, trailing `_` for
  private members, `k`-free plain constants. Algorithms use real names (`appendSubtractiveDivideWhile`).
- Namespace `algosearch`; anonymous namespaces for file-local helpers. Prefer `const` refs.
- No formatter or linter is configured: match surrounding code and keep `-Wall -Wextra` clean.

## Testing Guidelines

Tests use the in-repo `testfw` framework (`tests/test_framework.hpp`): `TEST(name)` with
`CHECK`/`CHECK_EQ`. Add a `*_tests.cpp` file (or extend an existing one) under `tests/`;
names are `snake_case` descriptive phrases, e.g. `sorting_problem_partial_score_penalizes_inversions`.
Run all 58 tests via `make test`; the binary exits non-zero on any failure. New problems need
instance-generation, `isSolution`, and known-solution tests.

## Commit & Pull Request Guidelines

Git history uses imperative, single-sentence subjects that state the change and its outcome,
sometimes with an "honest negative result" qualifier (`Add TSP problem with brute-force and
branch-and-bound skeletons`). Keep subjects under ~72 characters; put detail in the body.

PRs should describe the change, link related `FINDINGS.md` sections, and report the exact
command plus seed/budget used to reproduce any claimed result. Include test output and, for
search-behavior claims, the raw `data/` run rather than a summary. Update `README.md` and
append to `FINDINGS.md` when adding problems, grammar primitives, or skeletons.
