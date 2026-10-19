# Findings index

Generated from the headers of `FINDINGS.md` as it stood at commit `5706b0c` (tag it with `pre-summary`). One line per section header: the chunk file under `docs/findings/` and the line inside it. Use `grep -n 'NNN' docs/findings/INDEX.md`, then read only that part of the chunk.

| header | file | line |
|---|---|---|
| 1. V1 worked, but "solved" needs care | 01_sections_001-017.md | 10 |
| 2. Parallelism and a real memory bug | 01_sections_001-017.md | 20 |
| 3. Sorting overfits badly on a naive instance set | 01_sections_001-017.md | 33 |
| 4. Generalization horizon > single SUCCESS/FAIL | 01_sections_001-017.md | 49 |
| 5. The core discovery: grammar prior >> population diversity | 01_sections_001-017.md | 69 |
| 6. Forensic structural analysis: why the bias works | 01_sections_001-017.md | 108 |
| 7. A single feature isn't independently causal -- the skeleton is | 01_sections_001-017.md | 129 |
| 8. Sorting: promising, but budget-mismatched so far | 01_sections_001-017.md | 161 |
| 9. Native Le/Ge/Ne close the mod/gcd "wall" -- at least once, honestly | 01_sections_001-017.md | 178 |
| Current defaults (as of this entry) | 01_sections_001-017.md | 265 |
| 10. Four new curriculum problems: count, average, reverse, power | 01_sections_001-017.md | 276 |
| 11. Consolidated 8 skeletons down to 6 | 01_sections_001-017.md | 421 |
| 12. Cross-skeleton study: does any problem converge on someone else's skeleton? | 01_sections_001-017.md | 466 |
| 13. Chasing better asymptotic complexity: gcd O(log n), is_prime O(sqrt n) -- both hit the same wall | 01_sections_001-017.md | 523 |
| 14. Sorting reaches O(n log n) -- heapsort, gated behind maxLoops=4 | 01_sections_001-017.md | 617 |
| 15. 3SUM: free search fails cold, a dedicated 3-index skeleton fixes it outright | 01_sections_001-017.md | 696 |
| 16. Where this sits relative to the academic/industrial state of the art | 01_sections_001-017.md | 772 |
| 17. Breaking the language ceiling for real: native Mul/Div/Mod/bitwise, plus two new problem families | 01_sections_001-017.md | 837 |
| 18. TSP, and the "middle ground" question: can capability grow without a human writing a new skeleton? | 02_sections_018-028.md | 1 |
| 19. DTW / Frechet-distance-shaped problem: a SETH-conjectured-optimal O(n^2), not just "best known" | 02_sections_018-028.md | 128 |
| 20. contains_ordered at a large budget + high diversity: still no binary search, matched A/B | 02_sections_018-028.md | 181 |
| 21. contains_ordered with ALL quota strategies + crossover + speed bonus: still no binary search | 02_sections_018-028.md | 229 |
| 22. Binary-search "ladder": does the tipping point arrive gradually, or all at once? | 02_sections_018-028.md | 303 |
| 23. Section 22's open question, answered: the skeleton never gets a chance to compete, regardless of speedBonusWeight | 02_sections_018-028.md | 391 |
| 24. Cataloguing the skeleton library: which primitives actually recur, and which are irreducible | 02_sections_018-028.md | 446 |
| Open threads, roughly in the order they were flagged as next steps | 02_sections_018-028.md | 538 |
| 25. Held-out discovery benchmark: three problems outside the skeleton library, and the baseline that freezes the starting line | 02_sections_018-028.md | 563 |
| 26. Divergence engine (Moção 1, Etapa 2): collects alternatives, still zero generalizers | 02_sections_018-028.md | 638 |
| 27. Composite primitives with binding (Moção 2, Etapa 3): the operator composes the shape, but the search never converges to the clean version | 02_sections_018-028.md | 687 |
| 28. Diversity battery: does the combination of both new mechanisms increase archived diversity across the ORIGINAL 21, and does it regress anything? | 02_sections_018-028.md | 798 |
| 29. Long-budget sorting run: real structural transition, inflated diversity counts, and a growth-corruption stall | 03_sections_029-038.md | 1 |
| 30. Why `genBinarySearchSkeletonBlock` never gets picked up unforced: two real generator bugs found and fixed, one probability wall left standing | 03_sections_029-038.md | 92 |
| 31. Substring isolation test: the gap is compositional (search), not expressive (VM), same two-phase protocol as TSP/DTW | 03_sections_029-038.md | 183 |
| 32. Scoped conjunction: adding the missing piece as a family capability, not a substring scaffold -- positive on two different problems | 03_sections_029-038.md | 272 |
| 33. Generation-time pruning of text-level trivia (phases 0+1): real junk, removing it is neutral | 03_sections_029-038.md | 391 |
| 34. Phase-3 go/no-go check: the bloat in a stalled sorting run is mostly live code, not dead weight -- phase 3 dropped | 03_sections_029-038.md | 481 |
| 35. Behavioral redundancy of generation: 70-98% of scored candidates behave exactly like something already seen | 03_sections_029-038.md | 551 |
| 36. Role-typed variables (idea C): new-behavior rate up 1.4-2.5x in the free grammar, crashes down; first gate passed, outcome not yet tested | 03_sections_029-038.md | 645 |
| 37. Vacuous SUCCESS at max_level, a metric correction, and the role-typed outcome A/B: no genuine gain | 03_sections_029-038.md | 722 |
| 38. Combinator algebra (idea B): 9 problems solved in isolation from a product of catalogued axes, one algorithm no skeleton contains -- and a second verification weakness | 03_sections_029-038.md | 844 |
| 39. Post-freeze test of the combinator algebra: 3 of 3 predicted-reachable new problems solved, 0 of 3 predicted-unreachable, and an exhaustive check in --benchmark | 04_sections_039-050.md | 1 |
| 40. Combinator v2: one new axis value (adjacent pairs) -- frozen, predictions registered BEFORE writing its test problems | 04_sections_039-050.md | 104 |
| 41. Idea A: filling the TSP pruning guard by exhaustive enumeration -- predictions registered before running | 04_sections_039-050.md | 132 |
| 40 (results). Combinator v2: every registered prediction held; plus search reaches beyond the v1 product occasionally | 04_sections_039-050.md | 209 |
| 42. Adjacent-pair axis promoted into the default combinator family (branch `combinator-adjacent-default`) -- test prepared, not yet run | 04_sections_039-050.md | 253 |
| 42 (results). Combinator on top of the skeleton library: every prediction held, no regression, two original problems improve | 04_sections_039-050.md | 285 |
| 43. TSP guard selection with per-evaluation soundness labels (branch `tsp-guard-soundness`) -- test prepared, not yet run | 04_sections_039-050.md | 316 |
| 43 (results). Soundness labels do NOT fix selection -- prediction 1 failed, for the reason caveat 3 anticipated | 04_sections_039-050.md | 355 |
| 44. Metamorphic weight scaling for TSP guard selection -- predictions registered before running | 04_sections_039-050.md | 389 |
| 45. Sorting diversity is skeleton diversity: with the three sort skeletons off, no general sort is found | 04_sections_039-050.md | 512 |
| 46. Sorting without sort skeletons, combinator on: 8/8 seeds solve, and bubble and gnome sort appear by composition | 04_sections_039-050.md | 582 |
| 47. Combinator as the default, against the TRUE current default -- predictions registered before running | 04_sections_039-050.md | 667 |
| 47 (results). Combinator vs the true default: no regression, 8 problems 0-4/15 -> 15/15, one registered bound missed | 04_sections_039-050.md | 701 |
| 48. Combinator on by default | 04_sections_039-050.md | 742 |
| 49. three_sum with high diversity and many seeds: only the skeleton, nothing without it | 04_sections_039-050.md | 767 |
| 50. Do solutions found on one problem become stepping stones for others? -- library frozen, 11 new problems, predictions registered before running | 04_sections_039-050.md | 806 |
| 50 (results, arms D and N). The falsifier fires: almost nothing solves even with a sort block, though composition after a block does happen | 04_sections_039-050.md | 869 |
| 51. Option (b): enumerating the small tail after a block -- predictions registered before running | 05_sections_051-063.md | 1 |
| 51 (results). Tail enumeration turns section 50's near misses into solutions: 0/15 -> 13/15, every prediction held | 05_sections_051-063.md | 39 |
| 52. Option (a): step axes in the combinator -- frozen, post-freeze problems written, predictions registered before running | 05_sections_051-063.md | 72 |
| 52 (results). Step axes: post-freeze problems 15/15, design targets reached by composing two blocks, no dilution | 05_sections_051-063.md | 113 |
| 53. Should `--tail-enum` and/or `--combinator-step` become defaults? -- predictions registered before running | 05_sections_051-063.md | 161 |
| 53 (results). `--combinator-step` becomes the default; `--tail-enum` does not (yet) | 05_sections_051-063.md | 190 |
| 54. `--tail-enum` on top of the new default -- predictions registered before running | 05_sections_051-063.md | 237 |
| 54 (results). `--tail-enum 10` becomes the default: median 0 -> 23/30, second_smallest 2 -> 15/30, no regression | 05_sections_051-063.md | 256 |
| 55. Stateful combinator pieces (Kadane, rain water, LIS) -- frozen, post-freeze problems written, predictions registered before running | 05_sections_051-063.md | 284 |
| 55 (results). Stateful pieces: every post-freeze problem and every design target solved, no dilution | 05_sections_051-063.md | 333 |
| 56. Stateful pieces as default + which hand-written skeletons are indispensable -- predictions registered before running | 05_sections_051-063.md | 371 |
| 56 (results). Isolation: 8 problems need their skeletons, 4 skeletons are removable; stateful default narrowly fails its rule | 05_sections_051-063.md | 415 |
| 57. Two skeletons removed: `counting` and `divideconquer` | 05_sections_051-063.md | 476 |
| 58. `--combinator-state` retest on its two near misses -- predictions registered before running | 05_sections_051-063.md | 502 |
| 58 (results). Not noise: `--combinator-state` halves graph_reachability, stays opt-in | 05_sections_051-063.md | 518 |
| 59. Adaptive family weights against dilution -- design and predictions registered before implementing | 05_sections_051-063.md | 544 |
| 59 (results, phase A). The bandit credits the right skeleton but does not fix the crowding; the 1/8 share does | 05_sections_051-063.md | 617 |
| 59 (results, phase B). ST8 solves every stateful target but misses no-regression again on graph_reachability | 05_sections_051-063.md | 660 |
| 60. A protected beam quota for skeleton lineages -- predictions registered before implementing | 05_sections_051-063.md | 705 |
| 60 (results). Quota refuted, and the regression it targeted is a step-limit artifact | 05_sections_051-063.md | 752 |
| 61. `--combinator-state` as the default, with the step limit corrected -- predictions registered before running | 05_sections_051-063.md | 803 |
| 61 (results). Success predictions hold; the 2x cost rule fails on sub-second problems, so the default does not change | 05_sections_051-063.md | 825 |
| 62. `--combinator-state` becomes the default by explicit decision, after section 61's data | 05_sections_051-063.md | 861 |
| 63. Section 22's binary-search ladder, stage 3, with the hole filled by enumeration -- predictions registered before implementing | 05_sections_051-063.md | 886 |
| 63 (results). Enumeration reaches binary search from stage 3; selection holds only by a tie-break, and the instance generator has a blind spot | 06_sections_063-073.md | 1 |
| 64. Stage 2: the found-check is a hole too -- predictions registered before running | 06_sections_063-073.md | 83 |
| 64 (results). Stage 2 reached: enumeration rebuilds the found-check and the decision together | 06_sections_063-073.md | 128 |
| 65. How much of each indispensable skeleton can enumeration rebuild? Single-hole ablation -- predictions registered before implementing | 06_sections_063-073.md | 166 |
| 65 (results). 100 of 112 skeleton sites are rebuilt by enumeration; what resists is compound array indexing, three-term sums, and selection on graph_reachability | 06_sections_063-073.md | 229 |
| 66. Shape-only skeletons + enumerative repair inside the search -- predictions registered before implementing | 06_sections_063-073.md | 301 |
| 66 (results). Shape alone does not compose: single-site repair from a scrambled skeleton reaches nothing; battery not run | 06_sections_063-073.md | 354 |
| 67. Joint enumeration of site pairs -- probe, prediction registered before running | 06_sections_063-073.md | 402 |
| 67 (results). Pairs move some climbs off zero but none reach a solution: 0/20 and 0/20 | 06_sections_063-073.md | 430 |
| 68. Filling all of a skeleton's holes at once with an SMT solver (sketching / CEGIS) -- pilot, predictions registered before implementing | 06_sections_063-073.md | 452 |
| 68 (results). The SMT encoding does not scale even to gcd's 7-hole sketch: 16/16 timeouts; pilot stopped for memory | 06_sections_063-073.md | 506 |
| 69. Closing the "shape vs fills" thread (sections 59-68), and the code clean-up | 06_sections_063-073.md | 556 |
| 70. Phase-1 reachability for the five never-solved problems -- prediction registered before running | 06_sections_063-073.md | 616 |
| 70 (results). All five are expressivity gaps: no sample of the default family solves any of them | 06_sections_063-073.md | 643 |
| 71. Nested aggregates and an absolute step -- design registered before implementing | 06_sections_063-073.md | 698 |
| 72. `--combinator-nested` becomes the default by explicit decision, after section 71's data | 06_sections_063-073.md | 845 |
| 73. Canonical outer tests for the nested aggregate -- predictions registered before implementing | 06_sections_063-073.md | 873 |
| 73 (results). count_unique 8 -> 10, but has_majority 8 -> 6: the no-regression rule fails by one | 07_sections_073-087.md | 1 |
| 74. Canonical tests: 30-seed retest of has_majority and count_unique -- predictions registered before running | 07_sections_073-087.md | 28 |
| 74 (results). Not noise: canonical tests cost has_majority 29 -> 22 of 30; they stay opt-in | 07_sections_073-087.md | 42 |
| 75. A frequency-table piece -- design registered before implementing | 07_sections_073-087.md | 72 |
| 76. Frequency piece: 30-seed retest of second_smallest -- prediction registered before running | 07_sections_073-087.md | 168 |
| 76 (results). Noise, as before: second_smallest 24 = 24 of 30; `--combinator-frequency` becomes the default | 07_sections_073-087.md | 180 |
| 77. A pointer-following piece -- design registered before implementing | 07_sections_073-087.md | 194 |
| 78. Follow piece: 30-seed retest of max_gap_sorted -- prediction registered before running | 07_sections_073-087.md | 299 |
| 78 (results). Not noise this time: max_gap_sorted 28 -> 23 of 30; `--combinator-follow` stays opt-in | 07_sections_073-087.md | 311 |
| 79. Gating the follow piece on pointer-like inputs -- predictions registered before running | 07_sections_073-087.md | 333 |
| 79 (results). The gate keeps every gain and removes the cost; `--follow-pointer-inputs` becomes the default | 07_sections_073-087.md | 372 |
| 80. Orthogonal modifiers: difference direction and absolute value as axes -- design registered before implementing | 07_sections_073-087.md | 406 |
| 81. `--combinator-orthogonal` becomes the default by explicit decision, after section 80's data | 07_sections_073-087.md | 505 |
| 82. Arg-best: returning *where* the best is, not its value -- design registered before implementing | 07_sections_073-087.md | 530 |
| 83. Arg-best: 30-seed retest of min_gap -- prediction registered before running | 07_sections_073-087.md | 628 |
| 83 (results). min_gap 22 -> 18 of 30: the bound is missed by one; arg-best stays opt-in | 07_sections_073-087.md | 640 |
| 84. Arg-best: 30-seed check of max_gap_sorted (diagnostic for section 83) -- prediction registered before running | 07_sections_073-087.md | 657 |
| 84 (results). max_gap_sorted 23 -> 24 of 30: no dilution of the shared recipe | 07_sections_073-087.md | 673 |
| 85. `--combinator-argbest` becomes the default by explicit decision, after sections 82-84 | 07_sections_073-087.md | 685 |
| 86. First-occurrence filter: counting values, not elements -- design registered before implementing | 07_sections_073-087.md | 705 |
| 87. Distinct counting, v2 -- design registered before implementing | 07_sections_073-087.md | 818 |
| 88. `--combinator-distinct` becomes the default by explicit decision, after section 87's investigation | 08_sections_088-100.md | 1 |
| 89. Correction to section 87's investigation, and a regression guard -- design registered before implementing | 08_sections_088-100.md | 28 |
| 89 (results). The guard removes every n = 1 rejection: min_gap 18 -> 30 of 30, no regression, no extra wall time; default | 08_sections_088-100.md | 71 |
| 90. Follow-piece extensions: trailing node and loop length -- design registered before implementing | 08_sections_088-100.md | 107 |
| 91. Cumulative time audit of the nine pieces promoted since section 61 -- design and expectations registered before running | 08_sections_088-100.md | 200 |
| 92. Long sorting run (5 h) after sections 45-91: heap sort in a minute, still two algorithm families, stall at N=37 | 08_sections_088-100.md | 287 |
| 93. Eight short sorting seeds beat one long run on algorithmic diversity | 08_sections_088-100.md | 334 |
| 94. `--restart-on-stall K` and `--wall-seconds S` (opt-in) -- implementation | 08_sections_088-100.md | 378 |
| 95. A/B: `--restart-on-stall` vs fixed short seeds -- design and predictions registered before running | 08_sections_088-100.md | 415 |
| 95 (results). Restarts reach heap sort in every run but add no family; stays opt-in | 08_sections_088-100.md | 444 |
| 96. Restart A/B on `max_subarray_sum` -- design and predictions registered before running | 08_sections_088-100.md | 480 |
| 96 (results). max_subarray_sum: only Kadane, in both arms; restarts multiply variants, not families | 08_sections_088-100.md | 518 |
| 97. `--confirm-reseed L` (opt-in): fresh-seed confirmation catches one of section 96's two control false positives | 08_sections_088-100.md | 562 |
| 98. Cost-profile novelty -- design registered before implementing | 08_sections_088-100.md | 602 |
| 97 (decision). `--confirm-reseed 12` is the default | 08_sections_088-100.md | 656 |
| 98 (implementation). `--profile-novelty W`, and the search pads to game it | 08_sections_088-100.md | 661 |
| 98 (results). Novelty compresses the archive 100x and finds heap sort in 8/8 seeds, but discovers no new family | 08_sections_088-100.md | 700 |
| 99. Skeleton re-isolation under the current default -- predictions registered before running | 08_sections_088-100.md | 759 |
| 99 (amended decision rule). Registered 2026-10-11 while the battery ran, before reading any result | 08_sections_088-100.md | 796 |
| 99 (results). No skeleton is removable: six drop to 0 without their skeleton, selectionsort fails by one seed | 08_sections_088-100.md | 827 |
| 100. Generic scalar-loop piece (`--scalar-loop`) -- design and predictions registered before implementing | 08_sections_088-100.md | 864 |
| 100 (implementation). `--scalar-loop`, and factorial's reference overflows past n = 12 | 09_sections_100-111.md | 1 |
| 100 (benchmark fix). factorial and power: exact 64-bit reference, overflow is out of domain | 09_sections_100-111.md | 34 |
| 100 (results). The scalar-loop piece replaces four of the five scalar skeletons at 10 seeds | 09_sections_100-111.md | 65 |
| 100 (confirmation). Four skeletons pass steps 3 and 5; joint battery launched | 09_sections_100-111.md | 107 |
| 100 (decision). Joint battery passes: `--scalar-loop` is default, four skeletons deleted | 09_sections_100-111.md | 139 |
| 101. Scalar-loop index range: can the piece replace `average`? -- design and predictions registered before implementing | 09_sections_100-111.md | 172 |
| 101 (results, part 1). Probe and 30-seed average A/B both pass | 09_sections_100-111.md | 211 |
| 101 (results, part 2). Joint battery passes; index range default, `average` deleted | 09_sections_100-111.md | 235 |
| 102. Nested placement piece (`--nested-place`): can the combinator replace `selectionsort`? -- design and predictions registered before implementing | 09_sections_100-111.md | 250 |
| 102 (results). Nested placement fails: min_gap 29 -> 21, selection family 30 -> 1; `selectionsort` kept | 09_sections_100-111.md | 287 |
| 102b. Placement as its own pair relation (`--nested-place-option`) -- design and predictions registered before implementing | 09_sections_100-111.md | 315 |
| 102b (results). Placement as its own relation still fails on min_gap (30 -> 21); `selectionsort` closed as indispensable | 09_sections_100-111.md | 342 |
| 103. Triple aggregate piece (`--combinator-triple`): can the combinator replace `threesum`? -- design and predictions registered before implementing | 09_sections_100-111.md | 370 |
| 103 (results, part 1). Probe and three_sum A/B pass | 09_sections_100-111.md | 399 |
| 103 (results, part 2). Joint battery passes; triple piece default, `threesum` deleted | 09_sections_100-111.md | 417 |
| 104. Matrix relaxation piece (`--combinator-relax`): can the combinator replace `graphreachability`? -- design and predictions registered before implementing | 09_sections_100-111.md | 429 |
| 104 (results, part 1). Probe and graph_reachability A/B pass | 09_sections_100-111.md | 465 |
| 104 (results, part 2). Joint battery passes; relax piece default, `graphreachability` deleted | 09_sections_100-111.md | 485 |
| 105. Does the relax piece also replace `shortestpath`? -- predictions registered before running | 09_sections_100-111.md | 505 |
| 105 (results). The `shortestpath` skeleton never solved its problem; deleted | 09_sections_100-111.md | 522 |
| 106. Winner diversity across the section 101/103/104 joint batteries (post-hoc analysis, no new runs) | 09_sections_100-111.md | 548 |
| 107. Isolation screening of the never-tested skeletons (dtw, tspbruteforce, tspbranchandbound) -- registered before running | 09_sections_100-111.md | 580 |
| 107 (results). dtw needs its skeleton; tsp is undecidable at this budget | 09_sections_100-111.md | 599 |
| 108. Isolation screening of binarysearch and heapsort; mergesort is inert in the default -- registered before running | 09_sections_100-111.md | 622 |
| 108 (results). binarysearch and heapsort contribute nothing visible in the default | 09_sections_100-111.md | 647 |
| 109. Grid DP piece (`--combinator-grid`): can the combinator replace `dtw`? -- design and predictions registered before implementing | 09_sections_100-111.md | 675 |
| 109 (results). Grid piece: dtw 30/30 without its skeleton; default, `dtw` deleted | 09_sections_100-111.md | 706 |
| 107b. tsp re-screen with a sufficient step limit -- registered before running | 09_sections_100-111.md | 727 |
| 107b (results). The search solves tsp in seconds; verification rejects every exact solver at n = 15 | 09_sections_100-111.md | 748 |
| 107c. tsp re-screen with verification capped at V <= 10 -- registered before running | 09_sections_100-111.md | 779 |
| 107c (results). Branch-and-bound alone matches base; brute force times out at V = 10 | 09_sections_100-111.md | 793 |
| 107d. tsp re-screen with verification capped at V <= 9 -- registered before running | 09_sections_100-111.md | 811 |
| 107d (results). At V <= 9 both tsp skeletons are independent routes | 09_sections_100-111.md | 822 |
| 110. Reference battery of the current main -- registered before running | 09_sections_100-111.md | 831 |
| 110 (results). Current main: 667/730, identical total to section 104 | 09_sections_100-111.md | 845 |
| 111. Per-term transform piece (`--combinator-transform`) -- registered before implementing | 09_sections_100-111.md | 861 |
| 111 (screen results). Every target 10/10, both rulers held | 09_sections_100-111.md | 893 |
| 111 (joint battery). Promoted: 687 vs 667, no other problem moved | 10_sections_111-121.md | 1 |
| 112. Distinct aggregate and parity filter (`--distinct-aggregate`) -- registered before implementing | 10_sections_111-121.md | 16 |
| 112 (screen results). Every target 10/10, both rulers held | 10_sections_111-121.md | 56 |
| 112 (joint battery). 703 vs 687, but two drops of >= 2: not promoted as is | 10_sections_111-121.md | 78 |
| 112b. Non-diluting redesign of `--distinct-aggregate` -- registered before running | 10_sections_111-121.md | 100 |
| 112b (results). Dilution fixed on the 73; max_repeated_value still 3 -> 0 | 10_sections_111-121.md | 121 |
| 112c. Diagnosis of the max_repeated_value loss: a deceptive attractor (logs only, no new runs) | 10_sections_111-121.md | 146 |
| 112d. Max aggregate on DistinctCount -- registered before running | 10_sections_111-121.md | 166 |
| 112d (results). Promoted: 704 vs 687, no drop >= 2, attractor gone | 10_sections_111-121.md | 188 |
| 113. Arg runs and sentinel guards (`--arg-sentinel`) -- registered before implementing | 10_sections_111-121.md | 213 |
| 113 (results). Promoted: 717 vs 704, no drop >= 2; guarded targets stay rare | 10_sections_111-121.md | 259 |
| 114. External held-out set: three problems from a public list -- registered before running | 10_sections_111-121.md | 288 |
| 114 (results). External set: 4 of 30; the calibration expectation failed | 10_sections_111-121.md | 322 |
| 115. Local comparison mutation (`--local-cond-mutation`) -- registered before running | 10_sections_111-121.md | 340 |
| 115 (results). Small, consistent gains; the guard prediction failed | 10_sections_111-121.md | 371 |
| 115b. 30-seed A/B of `--local-cond-mutation 0.3` -- registered before running | 10_sections_111-121.md | 396 |
| 115b (results). Promoted: 44 vs 21 of 150 | 10_sections_111-121.md | 408 |
| 116. Diagnosis of equilibrium_point (user request; it stops being an independent held-out problem) | 10_sections_111-121.md | 429 |
| 117. Closing section 116's two gaps (`--first-index`, `--prefix-split`) -- registered before implementing | 10_sections_111-121.md | 465 |
| 117 (results). Gaps closed in the screen; three regressions in the joint battery | 10_sections_111-121.md | 508 |
| 117b. Attribution A/B of the three drops -- registered before running | 10_sections_111-121.md | 541 |
| 117b (results). PrefixSplit, not FirstIndex, causes the frequent_mode_or_none drop | 10_sections_111-121.md | 553 |
| 118. Attractor ban (`--profile-ban`): diversity A/B -- registered before running | 10_sections_111-121.md | 575 |
| 118 (results). The ban adds at most two classes per seed, never a family; stays opt-in | 10_sections_111-121.md | 612 |
| 118b (results). Sorting without swap re-selects the hand-written merge sort skeleton | 10_sections_111-121.md | 654 |
| 118c (results). Sorting without swap and without the merge skeleton: 0 generalizing programs | 10_sections_111-121.md | 678 |
| 119. Generic divide-and-conquer shell (`--dc-shell`) -- design registered before implementing | 10_sections_111-121.md | 694 |
| 119 (amendment, before the A/B). The smoke run shows the loop wins the size race | 10_sections_111-121.md | 748 |
| 119 (results). The shell never produces a recursive reduction: prediction 1 falsified | 10_sections_111-121.md | 768 |
| 120. Open-specification problems: the checker is a property, not one reference answer -- design registered before running | 10_sections_111-121.md | 802 |
| 120 (amendment, before any valid result). Level 1 is trivial: rerun with `--start-level 2` | 10_sections_111-121.md | 837 |
| 120 (results). any_peak: 10/10 and six behavior classes; no sublinear peak; any_duplicate stalls | 10_sections_111-121.md | 847 |
| 121. One-call interval shell (`--dc-shell-search`) on any_peak -- design registered before running | 10_sections_111-121.md | 892 |
| 121 (results). The one-call shell finds binary-search peak in 9 of 10 seeds; the loop-free arm finds nothing | 11_sections_121-125.md | 1 |
| 121b. Second problem for the one-call shell -- predictions registered before running | 11_sections_121-125.md | 49 |
| 121b (results). contains_ordered 0 hits as predicted; any_valley 9/10 hits | 11_sections_121-125.md | 80 |
| 121c. Key-based interval shell (`--dc-shell-search-key`) -- design and predictions registered before running | 11_sections_121-125.md | 97 |
| 121c (results). Key mode assembles an O(log n) membership search in 5 of 10 seeds (8 by the engine's check) | 11_sections_121-125.md | 132 |
| 122. Step 1 of 3 toward a readable result: adversarial haystacks of every size -- registered before running | 11_sections_121-125.md | 187 |
| 122 (results). Prediction held: the engine now agrees with the outside check | 11_sections_121-125.md | 214 |
| 123. Step 2 of 3: a third search shape, the minimum of a rotated sorted array -- design registered before running | 11_sections_121-125.md | 223 |
| 123 (results). Negative: no hit on rotated_min; the claim stays at two specifications | 11_sections_121-125.md | 261 |
| 124. Step 3 of 3: joint battery with the interval piece on -- design registered before running | 11_sections_121-125.md | 289 |
| 124 (results). The piece costs nothing measurable and changes the selected solution on three problems | 11_sections_121-125.md | 318 |
| 125. Cleanup, group 1: redundant positive flags removed | 11_sections_121-125.md | 374 |
| 125b. Cleanup, group 2: six dead levers removed (and a correction to the proposal) | 11_sections_121-125.md | 388 |
| 125c. Cleanup, groups 3 and 4: four closed experiments removed, the rest kept on evidence | 11_sections_121-125.md | 410 |
| 126. PSB2, the comparable baseline -- design and predictions registered before running | 11_sections_121-125.md | 437 |
| 126r. PSB2 first full run: gcd 30/30 (contaminated), paired_digits 4/30, other five 0/30; fuel_cost gap vs PushGP | 11_sections_121-125.md | 456 |
| 126b. Fuel cost diagnosis: known solution passes, search never composes x/3-2 | 11_sections_121-125.md | 507 |
| 127. Term-expression piece (`--term-expr P`), opt-in; screen 4/8, predictions registered | 11_sections_121-125.md | 520 |
| 128. Other unsolved PSB2 problems: known solutions pass, search stuck on a plateau (diagnosis) | 11_sections_121-125.md | 532 |
| 128b. Prefix-crossing piece (`--prefix-cross P`), opt-in; basement 0/30 -> 30/30 | 11_sections_121-125.md | 550 |
| 129. Term run results (fuel_cost 29/30, paired_digits 2/30) and the joint battery, registered before running | 11_sections_121-125.md | 562 |
