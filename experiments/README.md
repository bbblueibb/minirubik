# Homework 1 experiment artifacts

This directory keeps small, reproducible experiment inputs and concise records of the measurements used in the Homework 1 write-up.

These files are supporting artifacts. The HackMD note remains the main written record, while the solver and RV32I source code belong in the repository itself.

## Files

- `sim_rate.s` — RV32I loop used to compare Ripes simulation rates.
- `memory_probe_64k.s` — RV32I loop used for the 64 KiB guest-memory control measurement.
- `memory_probe_1m.s` — RV32I loop used for the 1 MiB guest-memory measurement.
- `results/stage1_measurements.md` — reconstructed summary of the Stage 1 measurements from the recorded terminal/screenshots.
- `results/stage2_benchmarks.md` — concise record of the Stage 2 IDA*/PDB benchmark results.

## Important note

The result files are reconstructed summaries from the measurements recorded during development. They are not claimed to be verbatim raw terminal transcripts. Future measurements should preferably be redirected to a file at the time they are run.
