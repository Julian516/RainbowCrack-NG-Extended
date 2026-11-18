<div align="center">

# RainbowCrack-NG Extended

**SHA-256 · Argon2id · Deterministic rainbow chains · Multithreaded CPU generation**

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)](https://isocpp.org/)
[![Linux](https://img.shields.io/badge/platform-Linux-informational)](#installation)
[![Experimental](https://img.shields.io/badge/status-experimental-orange)](#current-status)

An independent extension of [RainbowCrack-NG](https://github.com/inAudible-NG/RainbowCrack-NG), focused on reproducible rainbow-table generation and CPU performance.

</div>

> [!IMPORTANT]
> Experimental software for research, education, and authorized security assessment. The Argon2id implementation currently uses fixed, source-defined parameters and a fixed test salt. It is **not** an example of secure password-storage configuration.

## Overview

RainbowCrack-NG Extended implements the classic rainbow-table workflow: generate hash/reduction chains, store their endpoints, sort the resulting table, and reconstruct candidate chains during lookup.

```text
plaintext x₀ → H(x₀) → R₀ → x₁ → H(x₁) → R₁ → ... → endpoint
```

The reduction function maps a digest back into the selected plaintext space; it does **not** reverse the hash. The stored `.rt` table contains the **starting index and endpoint index** for each chain, not the full intermediate sequence.

## Features

| Component | Status | Details |
| --- | --- | --- |
| SHA-256 | Implemented, tested | OpenSSL; 32-byte digest |
| Argon2id | Implemented, tested | `argon2idm4t2p1`: 4 MiB, 2 iterations, p=1, 32-byte digest |
| Legacy hash routines | Inherited | Including SHA-1 and MD5 |
| Rainbow chain generation | Working | Position-dependent reductions |
| Table sorting and lookup | Working | `rtsort`, `rcrack` |
| Multithreaded generation | Experimental | `--threads N` (1–64); tested with 1, 2, 4 threads |
| Deterministic starting indices | Experimental | Fixed-seed permutation; unique within one table when count ≤ plaintext space |
| Configurable Argon2id costs and salt | Not yet implemented | Source-defined test configuration |
| Configurable generation seed and chain ranges | Not yet implemented | Planned |
| Compact table format | Not yet implemented | Uses legacy `.rt` files |

Unique starting indices do not prevent internal revisits or chain merges. Equal endpoints are **not** evidence of cryptographic hash collisions.

## Installation

Tested on Debian 13 and in a Linux environment under WSL.

```bash
sudo apt update
sudo apt install -y git build-essential libssl-dev libargon2-dev

git clone https://github.com/Julian516/RainbowCrack-NG-Extended.git
cd RainbowCrack-NG-Extended/src
make clean
make -j4
```

Executables:

| Command | Purpose |
| --- | --- |
| `rtgen` | Generate rainbow tables |
| `rtsort` | Sort a generated table |
| `rcrack` | Search hashes using sorted tables |
| `rtdump` | Inspect table records |

If linking fails with `undefined reference to argon2id_hash_raw`, check that the executable targets link against `-largon2` after the object files. OpenSSL 3 may also report deprecation warnings for legacy routines such as MD4; those warnings are unrelated to SHA-256 or Argon2id.

## Quick start

Run from `src/` so that `charset.txt` is accessible. Each command uses a distinct suffix to avoid resuming an earlier output file.

### SHA-256

```bash
./rtgen sha256 numeric 1 4 0 100 1000 demo256 --threads 4
./rtsort 'sha256_numeric#1-4_0_100x1000_demo256.rt'
./rcrack 'sha256_numeric#1-4_0_100x1000_demo256.rt' \
  -h 03ac674216f3e15c761ee1a5e255f067953623c8b388b4459e13f978d7c846f4
```

The sample is the SHA-256 digest of `1234`. A lookup may return `<notfound>` if the table does not cover that plaintext.

### Argon2id

```bash
./rtgen argon2idm4t2p1 numeric 1 4 0 10 10 demoargon --threads 4
./rtsort 'argon2idm4t2p1_numeric#1-4_0_10x10_demoargon.rt'
```

The available routine is `argon2idm4t2p1` (without underscores), because `_` separates fields in the legacy table filename parser. Its parameters are currently compiled into `HashAlgorithm.cpp`, including a **fixed 16-byte test salt**. Generate a matching reference digest using those exact source-defined values before testing `rcrack`; arbitrary Argon2id digests are not interchangeable when the salt or costs differ.

### Fixed-length lowercase passwords

```bash
./rtgen sha256 loweralpha 5 5 0 100 1000 lower5 --threads 4
./rtsort 'sha256_loweralpha#5-5_0_100x1000_lower5.rt'
```

Here, `loweralpha 5 5` selects exactly five characters from `a` to `z`, giving **26⁵ = 11,881,376** candidate plaintexts. This command generates a small table; it does not cover the entire space.

## CLI

```text
rtgen HASH CHARSET MIN_LEN MAX_LEN TABLE_INDEX CHAIN_LENGTH CHAIN_COUNT SUFFIX [--threads N]
```

- **HASH:** registered algorithm (`sha256`, `argon2idm4t2p1`, etc.).
- **CHARSET:** named alphabet in `charset.txt` (`numeric`, `loweralpha`, etc.).
- **MIN_LEN / MAX_LEN:** plaintext length bounds.
- **TABLE_INDEX:** reduction-table index.
- **CHAIN_LENGTH:** length of each generated chain.
- **CHAIN_COUNT:** number of chains to generate.
- **SUFFIX:** identifies an output file.
- **--threads N:** optional CPU thread count; default is 1.

Files follow the naming scheme:

```text
HASH_CHARSET#MIN-MAX_INDEX_LENGTHxCOUNT_SUFFIX.rt
```

The legacy file format records two 64-bit indices per chain (**16 bytes per record**). Thus 1,000 chains occupy 16,000 bytes before filesystem overhead.

## Deterministic generation and threading

Threads work on separate chain contexts. A single writer collects each batch's results in a stable order. The current implementation assigns start indices through a fixed-seed affine permutation:

\[
\operatorname{start}(i) = (a i + b) \bmod N, \qquad \gcd(a,N)=1.
\]

For a fixed configuration and a chain count no greater than `N`, this avoids repeated starting indices within the table. With the tested SHA-256 configuration, files produced with **1 thread and 4 threads were byte-identical**.

**Limitations:** the seed is currently hardcoded; there is no configurable global starting range; resume behavior with partially generated tables needs further tests; and boundary conditions for very small plaintext spaces require extra validation.

## Preliminary performance

A single WSL test used Argon2id (`m=4 MiB`, `t=2`, `p=1`) and 100 chains of length 100:

| CPU threads | Throughput | Speedup |
| ---: | ---: | ---: |
| 1 | 2.64 chains/s | 1.00× |
| 2 | 4.71 chains/s | 1.78× |
| 4 | 8.17 chains/s | 3.09× |

These are preliminary single-run measurements, **not** controlled hardware benchmarks. In one deterministic SHA-256 table containing 100 chains, all 100 starts were unique, while only 68 endpoints were unique. Endpoint duplication does not directly quantify exact coverage.

## Source layout

```text
src/
├── HashAlgorithm.cpp        # Hash implementations
├── HashRoutine.cpp          # Algorithm registration and digest sizes
├── ChainWalkContext.cpp     # Plaintext conversion, chain walks, reductions
├── RainbowTableGenerate.cpp # Generation and threading
├── RainbowTableSort.cpp     # Table sorting
├── CrackEngine.cpp          # Lookup and chain reconstruction
├── RainbowCrack.cpp         # Lookup command-line application
├── charset.txt              # Named alphabets
└── Makefile                 # Linux build
```

## Roadmap

- [ ] Make Argon2id parameters and salt configurable and record them with each table.
- [ ] Add configurable seeds and non-overlapping global chain ranges.
- [ ] Add automated generation/lookup regression tests and concurrency checks.
- [ ] Improve metrics for chain revisits, endpoint duplication, effective coverage and lookup cost.
- [ ] Evaluate additional reduction strategies and compact table formats only after baseline measurements.

## Attribution

Based on [inAudible-NG/RainbowCrack-NG](https://github.com/inAudible-NG/RainbowCrack-NG) and the original RainbowCrack by **Zhu Shuanglei**, implementing rainbow-table techniques associated with **Philippe Oechslin**. Preserve upstream copyright notices and review the original licensing terms before redistribution. No new license is asserted for inherited source code here.
