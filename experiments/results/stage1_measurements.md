# Stage 1 measurement record

This file is a reconstructed summary of measurements recorded during development. It is not a verbatim raw transcript.

## Environment

- Host OS: Windows 11
- Linux environment: WSL2 Ubuntu 24.04 LTS
- Ripes: `Ripes-v2.2.6-106-g5b8a616-win-x86_64`
- ISA: RV32I
- Processors used for simulation-rate comparison: `RV32_ISS`, `RV32_5S`

## Baseline correctness

`make check` produced:

```text
3674160 states; diameter 11
8 solution vectors matched by solver and mini
invalid input rejected with status 2, unwritable stdout with status 1
```

For the provided distance-11 state:

```text
input:  21345671111111
output: B' R' D2 R' B R B' R D2 B R'
length: 11 moves
```

## Ripes host-memory experiment

Ripes was restarted before each measurement. Windows PowerShell process memory was recorded before and after the RV32I program wrote the requested guest-memory region.

PowerShell command used for process-memory inspection:

```powershell
Get-Process Ripes | Select-Object Id,
@{N='WorkingSetMiB';E={[math]::Round($_.WorkingSet64/1MB,2)}},
@{N='PrivateMiB';E={[math]::Round($_.PrivateMemorySize64/1MB,2)}}
```

Recorded private-memory measurements:

| Run | Guest region | Private before | Private after | Increase |
|---|---:|---:|---:|---:|
| 1 | 64 KiB | 23.47 MiB | 29.04 MiB | 5.57 MiB |
| 1 | 1 MiB | 23.22 MiB | 106.93 MiB | 83.71 MiB |
| 2 | 64 KiB | 23.79 MiB | 28.36 MiB | 4.57 MiB |
| 2 | 1 MiB | 22.84 MiB | 106.63 MiB | 83.79 MiB |

Slope calculation:

```text
ratio = (delta_large - delta_small) / (1 MiB - 64 KiB)
```

Results:

- Run 1: 83.35 host bytes / guest byte
- Run 2: 84.50 host bytes / guest byte
- Average: 83.93 host bytes / guest byte

Using the baseline peak guest-memory footprint of 18,405,414 bytes gives a projected host-memory cost of approximately 1.44 GiB on this Ripes build.

## Ripes simulation-rate experiment

The same `sim_rate.s` workload was run with Ripes CLI using `--iret` and `--exectime`.

Recorded results:

| Processor | Retired instructions | Execution time | Simulation rate |
|---|---:|---:|---:|
| RV32_ISS | 900,005 | 47 ms | 19.15 M instr/s |
| RV32_5S | 900,005 | 3,807 ms | 236.4 K instr/s |

The measured RV32_ISS rate was about 81 times the RV32_5S rate for this workload.

Example CLI pattern used during the measurement:

```powershell
Start-Process -FilePath $ripes `
  -ArgumentList "--mode","cli","--src",$src,"-t","asm","--proc","RV32_ISS","--iret","--exectime","--runinfo","--output",$out `
  -Wait
```

The RV32_5S run used the same source and options with `--proc RV32_5S`.
