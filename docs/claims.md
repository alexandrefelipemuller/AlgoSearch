# Claims, evidence and caveats (state of 2026-10-22, after section 138)

Every row points to the section of `docs/findings/` that holds the raw numbers. "Can say" is what the evidence supports today;
"cannot say" is what it does not. Nothing here is a result that has not been measured with matched seeds.

| # | Claim | Evidence | Can say | Cannot say / caveat |
|---|---|---|---|---|
| 1 | A search with no ML and no LLM solves four of the seven integer PSB2 problems in every one of 100 runs, two at about 90%, one at 14% | 139: 100 fresh seeds, 900 s, 200 train + 2,000 unseen cases, official data. gcd 100, basement 100, find_pair 100, paired_digits 100, fuel_cost 91, luhn 90, coin_sums 14 (595/700; the 30-seed figures of 126/131 said 178/210 and "6 of 7 at >= 93%", which 100 seeds do not support) | The rates above, on those seven problems, under the PSB2 success criterion | Only 7 of 25 PSB2 problems (the rest need floats or free strings, which the language cannot express). One configuration, no tuned comparison arm. gcd is contaminated (its algorithm is in the base). coin_sums is 14%, a miss |
| 2 | The gain over the baseline is 34/210 -> 178/210 | 126 (baseline) vs 131 | The six pieces turn the seven problems from 34 to 178 solved runs | The pieces were designed after looking at where the search failed on these same seven problems. This is fitting to the evaluation set, not a held-out result |
| 3 | The six pieces cost nothing on the rest of the suite | 129, 131: 77-problem battery, 10 matched seeds: 755 vs 755 (term+prefix), 759 vs 755 (four low-P pieces), max drop 1 | No per-problem drop of 2 or more was found, aggregate not worse | The aggregate has about +-3 run-to-run noise (wall-time limited runs), so the +4 is not a gain. High-P versions did cost (elements_twice 29 -> 24/30 at p about 0.09, section 131), which is why the defaults are low |
| 4 | The pieces generalize to unseen problems | 132-135: 17 problems the pieces were not designed for (PSB2 cut_vector, fizz_buzz, mastermind, twitter, middle_character, solve_boolean, leaders; ten PSB1-style, twice) | **No.** ON = OFF within noise everywhere (largest: middle_character 2/20 vs 0/20, p about 0.24); no harm either | Several of those problems are degenerate (cut_vector: output always 1) or out of reach for both arms (0/20). PSB1-style ones use my own generators, not the official data. The medium version was chosen after seeing the first result |
| 5 | A narrow generic form closes a real gap: `--count-mod` (count items with `x % c ==/!= 0`) | 136-138: count_odds, count_multiples_of_three, count_evens, count_not_multiple_of_four, BASE 0/80 -> MOD 63/80; control `x % 5 == 2` 0/20 -> 0/20; battery 758 vs 755 | The form is necessary and sufficient for that family in this search, at no measured cost on the 77 problems | One family, chosen as siblings on purpose, two of them motivated the piece; written by me, not an external benchmark; not PSB2. The wide version (`--count-term`) failed at the same P, so the draw space mattered more than the idea |
| 6 | `--square-term` closes sum_of_squares (10/10 at P 0.3) | 137 reach run | The gap (no i*i term in the scalar-loop Fold) is real and closable | Opt-in, not promoted: it came bundled with `--count-term`, whose battery dropped majority_value 5 -> 3; `--square-term` alone was not isolated |
| 7 | Comparison with published PushGP on PSB2 | PSB2 paper (Helmuth and Kelly 2021, ar5iv table, read 2026-10-22): PushGP+lexicase, 100 runs, 200 train / 2,000 test, 60M executions: GCD 8, Fuel Cost 50, Basement 1, Find Pair 4, Luhn 0, Paired Digits 8, Coin Sums 2. Ours (139): 100, 91, 100, 100, 90, 100, 14 | On the same protocol and the same seven problems our rates are higher on all seven (the biggest gaps: Basement 100 vs 1, Luhn 90 vs 0) | The published PushGP uses one untuned configuration; ours has pieces designed on these seven problems (row 2) and a wall-time budget as well as the 60M cap. On problems outside the integer subset PushGP is better: Middle Character 57/100 vs our at most 2/20 (own encoding), Fizz Buzz 25/100 vs 0/30, Cut Vector and Mastermind 0 for both. Our encodings are not the official format |
| 8 | Discovery of new or curious algorithms | `FINDINGS.md`, earlier sections | Not claimed by this table | Nothing in sections 126-138 is a new algorithm: they reproduce known solutions (fuel cost, running sum, pair search, digit fold) |

## What is missing for a stronger paper claim

1. 100 seeds on the seven PSB2 problems, and an equal-effort comparison arm (tuned baseline, same budget).
2. More PSB2 problems: needs a language extension (strings, floats), a large piece of work that changes the scope.
3. An independent held-out set: problems chosen by someone else, before any design work.
4. A fix for the weakest link of row 2, either by showing the pieces were not tuned to the seven, or by dropping the claim that they were found without looking.

## Rules this table follows

- Predictions registered before the runs; failures reported (for example sections 132, 133, 135, 137 each failed at least one).
- Matched seeds and a no-drop-of-2 rule before any default changes.
- `data/` is not committed; the commands and seeds are in each section.
