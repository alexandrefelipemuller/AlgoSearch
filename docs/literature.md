# Literature positioning (first pass, 2026-10-18)

Status: built from web-search result summaries, NOT from the full papers. Every claim must be
checked in the paper before it is cited. Decision of the project owner: we do not want to be close to
LLM-guided work (FunSearch, AlphaEvolve, OpenEvolve, CodeEvolve); we do want to be close to anyone
with a comparable non-neural approach (GP, enumeration, sketching, library learning), and to have a
comparable base of numbers (see PSB2 below).

## Comparison dimensions
| # | Dimension | Here | Elsewhere |
|---|---|---|---|
| M1 | success rate per problem | seeds solved / 10 | runs solved / 100 (PSB2) |
| M2 | budget to solve | candidates, seconds | program executions (PSB2 cap: 60 million) |
| M3 | generalization | independent benchmark, adversarial set, 400 outside inputs | unseen test cases (PSB2: 2,000) |
| M4 | complexity of what is found | fitted step exponent, O(.) class | rarely measured; ReSyn/SynPlexity take it as a spec |
| M5 | diversity of solutions | behavior classes, cost profiles | novelty search, entropy, distinct outputs |
| M6 | human knowledge built in | skeletons, hand-written pieces | learned libraries (DreamCoder), grammars, LLMs |
| M7 | new vs classic algorithm | manual label | only AlphaEvolve/FunSearch claim novelty |

## Ready-made searches (Google Scholar, dblp, arXiv, gpbib.cs.ucl.ac.uk)
1. Benchmark: `PSB2 PushGP lexicase generalization`; `program synthesis benchmark suite genetic programming`
2. Loops and recursion: `evolving recursive programs genetic programming`; `evolving efficient list search algorithms logarithmic`; `synthesis of loops from input-output examples`; `divide and conquer program synthesis schema`
3. Enumerative: `bottom-up enumerative program synthesis observational equivalence`; `guided bottom-up search Probe`; `SyGuS enumerative cvc5`
4. Sketching: `sketching CEGIS holes enumeration`; `Rosette solver-aided`
5. Diversity: `novelty search program synthesis lexicase`; `quality diversity program synthesis`; `diverse correct programs same specification`
6. Discovery: `FunSearch AlphaEvolve algorithm discovery`; `AlphaDev sorting`; `evolving sorting algorithms genetic programming`
7. Cost: `resource-guided synthesis`; `asymptotically efficient program synthesis`; `stochastic superoptimization`
8. Library learning: `DreamCoder library learning`; `Stitch library learning`

## Nearest work and distance
| Work | What it does | Shared with us | Distance |
|---|---|---|---|
| PSB2 (Helmuth, Kelly, GECCO 2021) https://ar5iv.labs.arxiv.org/html/2106.06086 | 25 problems; PushGP + lexicase; 100 runs, 200 train / 2,000 test cases, 60M-execution cap; mean success 10/100, 12 problems never solved | GCD is in both; success per problem, out-of-sample test | near; the comparable baseline |
| Sobania, EuroGP 2021 (single author on gpbib; earlier note said "and Rothlauf": wrong) https://gpbib.pmacs.upenn.edu/gp-html/Sobania_2021_EuroGP.html ; Helmuth et al., GECCO 2017 https://gpbib.pmacs.upenn.edu/gp-html/Helmuth_2017_GECCO.html | lexicase overfits on Boolean outputs; extra time after the first solution helps; smaller programs generalize better | post-solve window, statement-deletion simplification, adversarial set | near (method) |
| Wolfson and Sipper, EA 2009 https://gpbib.pmacs.upenn.edu/gp-html/Wolfson_2009_EA.html | GP evolves list search, including logarithmic time, with ADFs | our sections 121-121c | very near; prior art of our O(log n) result |
| Phillips et al. CEC 2016 https://gpbib.pmacs.upenn.edu/gp-html/Phillips_2016_CEC.html ; Agapitos and Lucas, EuroGP 2007 https://gpbib.pmacs.upenn.edu/gp-html/eurogp07_agapitos2.html | GP with recursion; recursive sorting coevolved | `call(lo, hi)`, merge sort | near |
| Jundt and Helmuth, GECCO 2019 https://ar5iv.labs.arxiv.org/html/1905.09374 ; Knobelty, EuroGP 2019 https://gpbib.cs.ucl.ac.uk/gp-html/Kelly_2019_EuroGP.html | novelty search + lexicase for program synthesis; novelty alone has little success | `--profile-novelty` (section 98) | near; agrees with our result |
| DreamCoder https://arxiv.org/pdf/2006.08381 ; Stitch https://arxiv.org/pdf/2211.16605 | library learning from solved programs | our skeleton -> generic piece replacement (100-109), but pieces are hand written | far (they learn, we design) |
| Probe https://arxiv.org/pdf/2310.04327 ; CAV 2024 https://arxiv.org/abs/2403.03997v2 | guided enumeration (SyGuS); LLM-guided enumerator wins | tail enumeration, hole filling (63-67) | far (loop-free expressions) |
| ReSyn https://arxiv.org/abs/1904.07415v1 ; SynPlexity https://link.springer.com/chapter/10.1007/978-3-030-81685-8_37 | synthesis against resource / big-O bounds | exponent measurement (M4) | far (bound is an input there, an output here) |
| Flener thesis 1993 https://webperso.info.ucl.ac.be/~yde/Papers/thesis-flener1993.pdf ; ESCHER https://www.microsoft.com/en-us/research/?p=335543 | divide-and-conquer schema; recursion from examples | the interval shell is a schema filled by draws | near in idea |
| STOKE https://ar5iv.arxiv.org/html/1211.0557 | MCMC superoptimization with a cost function | cost profile | partial |
| AlphaDev https://deepmind.google/blog/alphadev-discovers-faster-sorting-algorithms/ ; OpenEvolve https://pypi.org/p/openevolve ; CodeEvolve https://arxiv.org/pdf/2510.14150v3 | neural / LLM guided search | goal only | very far, by choice |

## Verdict
Not pioneers in synthesis-by-search; our O(log n) result has prior art (Wolfson and Sipper 2009).
Possible own contributions, to be checked against the papers: the protocol (predictions registered
before running, decision rules, published negatives, outside verification); the reachability map of
generic pieces and skeleton ablations; open-specification problems with behavior classes as the
diversity measure; a non-neural search with low evaluation budgets (section 16 of the old log).
To make any of it comparable: the PSB2 adapter (section 126 of the log) and reading PSB2, Wolfson
and Sipper, and Jundt and Helmuth in full.
