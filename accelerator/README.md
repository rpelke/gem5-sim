# LinearFunctionAccelerator

The `LinearFunctionAccelerator` is an MMIO peripheral for gem5 that computes a linear function on a 4-lane `int32` vector:

`y_i = a * x_i + b` for `i = 0..3`

The computation uses 64-bit intermediate arithmetic and writes results back with 32-bit wrap-around (modulo `2^32`, interpretable as two's-complement `int32`).

## Functionality

- 64-bit control register containing coefficients `a` and `b`.
- 128-bit input vector (`4 x int32`).
- 128-bit output vector (`4 x int32`).
- After a valid input transfer, the output vector is updated immediately.

## MMIO Interface

By default, the device is attached in the board configuration with:

- Base address: `0x1C150000`
- Window size: `0x30`
- PIO latency: `10ns`

Offsets relative to the base address:

- `0x00`: Control register (`uint64`)
- `0x10`: Input vector (`4 x int32`, total 16 bytes)
- `0x20`: Output vector (`4 x int32`, total 16 bytes)

### Control Register (`0x00`)

Bit layout:

- `bits[31:0]`  -> `a` (as `int32`)
- `bits[63:32]` -> `b` (as `int32`)

Accesses:

- Read: `32` bits (low/high word) or full `64` bits.
- Write: `32` bits (low/high word) or full `64` bits.

### Input Vector (`0x10`)

Supported write modes:

- Full write of 16 bytes at `0x10`: triggers computation immediately.
- Per-lane writes of 4 bytes at:
  - `0x10` (Lane 0)
  - `0x14` (Lane 1)
  - `0x18` (Lane 2)
  - `0x1C` (Lane 3)

With per-lane writes, computation is triggered only after all four lanes have been written at least once since the last computation.

### Output Vector (`0x20`)

Supported read modes:

- Full read of 16 bytes at `0x20`.
- Per-lane reads of 4 bytes at:
  - `0x20` (Lane 0)
  - `0x24` (Lane 1)
  - `0x28` (Lane 2)
  - `0x2C` (Lane 3)

## Valid and Invalid Accesses

Unsupported offsets or access sizes cause a `panic` in the model (for example, invalid width on a register region).

## Integration in gem5

The accelerator is integrated as an out-of-tree SimObject via `EXTRAS` (see `accelerator/SConscript`) and can be enabled in the board setup using `--linear-function-accelerator`.
