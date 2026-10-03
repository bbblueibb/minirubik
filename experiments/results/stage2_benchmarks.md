# Stage 2 benchmark record

This file records the host-side IDA*/PDB measurements used while selecting the Stage 2 design.

## Initial heuristic

Heuristic:

```text
h = max(permutation PDB, orientation PDB)
```

All 2,644 distance-11 states were evaluated.

| Metric | Result |
|---|---:|
| Minimum nodes | 140,901 |
| Average nodes | 206,624 |
| Maximum nodes | 639,798 |
| Worst-case rank | 2,437,047 |
| Worst-case state | `54721631111111` |
| Host elapsed time | 1.87 s |

For `21345671111111`:

```text
bound = 11
iterations = 5
nodes = 233,966
pruned = 194,967
solution length = 11
```

## After adding the 4-corner PDB

Heuristic:

```text
h = max(permutation PDB, orientation PDB, 4-corner PDB)
```

| Metric | Result |
|---|---:|
| Minimum nodes | 14,328 |
| Average nodes | 29,956 |
| Maximum nodes | 118,774 |
| Worst-case rank | 2,122,780 |
| Worst-case state | `51342763312223` |
| Host elapsed time before per-node optimization | 4.07 s |

For `21345671111111`:

```text
bound = 11
iterations = 5
nodes = 36,548
pruned = 30,452
solution length = 11
```

## After per-node optimization

The 4-corner partial-permutation ranking was rewritten using fixed comparisons/arithmetic, and pattern position updates used `destination_of[3][7]` instead of a search loop.

The search-node counts remained identical:

| Metric | Result |
|---|---:|
| Minimum nodes | 14,328 |
| Average nodes | 29,956 |
| Maximum nodes | 118,774 |
| Worst-case rank | 2,122,780 |
| Worst-case state | `51342763312223` |
| Host elapsed time | 1.24 s |

## Static-table prototype

The runtime-generated transition/PDB data were moved to offline-generated static tables. The static-table solver preserved the same search-node statistics.

Measured host binary sections:

| Section | Size |
|---|---:|
| `.rodata` | 109,312 B |
| `.data` | 16 B |
| `.bss` | 80 B |
| Total static data | 109,408 B (106.84 KiB) |

128 KiB limit:

```text
131,072 B
```

Remaining margin:

```text
21,664 B (21.16 KiB)
```

These host-side section figures are supporting measurements; the final RV32I build must be checked separately.
