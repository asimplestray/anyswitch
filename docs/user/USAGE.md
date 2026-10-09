# AnySwitch User Guide

## Quick Start

```bash
# Prerequisites (Linux)
sudo apt install cmake g++ ninja-build llvm-dev zlib1g-dev

# Clone AnySwitch
git clone https://github.com/anyswitch/anyswitch.git
cd anyswitch

# Remill/LLVM are optional (translation disabled without them).
# See 3rdparty/README.md for details.
# Or use system Remill: cmake -DANYSWITCH_USE_SYSTEM_REMILL=ON ...

# Configure and build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel --target relinker libnx libc libkernel

# Or just build everything
cmake --build build --parallel
```

## Porting a Game

### Step 1: Decrypt your game

You need a decrypted Switch game (`.nso` files). Use `hactool`:

```bash
hactool --intype nsp --extract-dir game_dump game.nsp
# or for XCI:
hactool --intype xci --extract-dir game_dump game.xci
```

You must provide your own `keys.txt` file. AnySwitch does not provide keys.

### Step 2: Prepare directory layout

```
game/
├── exefs/
│   ├── main.nso          # Main executable (ARM64)
│   └── subsdk0.nso       # Optional sub-libraries
├── romfs/                 # Game assets
│   ├── ...
└── ...
```

### Step 3: Convert

```bash
# The relinker translates ARM64 → x86-64 and patches the binary.
# Input can be a decrypted main.nso (NSO0, uncompressed or LZ4), a homebrew
# .nro (NRO0, plaintext — no decryption needed), or an ARM64 ELF64.
# ZBIC-compressed NSOs and hash verification are not supported.
./build/core/relinker/relinker \
    --input game/exefs/main.nso \
    --output converted_game/game.elf \
    --rpath '$ORIGIN/libs'

# Homebrew example (Checkpoint.nro, validated against v5.2.0):
./build/core/relinker/relinker Checkpoint.nro checkpoint.elf
```

### Step 4: Assemble runtime

```bash
# Copy system libraries
cp build/core/libs/libs/*.prx converted_game/libs/

# Copy game assets
cp -r game/romfs/* converted_game/app0/

# Run
./converted_game/game.elf
```

## Options

```
relinker [options] <input.nso> <output>

Options:
  --rpath <path>        Library search path (default: $ORIGIN/libs)
  --to-intel            Apply Intel-specific optimizations
  --registry            Write call registry JSON
  --lazy-binding        Use lazy symbol binding (not recommended for games)
  --skip-mtxs          Skip matrix co-processor instructions
  unused-filter=0|1|2   Remove unused library imports (0=none, 1=basic, 2=strict)

```

## Output Layout

```
converted_game/
├── game.elf          # Main executable (translated to x86-64)
├── libs/
│   ├── libc.prx      # Host libc
│   ├── libkernel.prx # Kernel services
│   ├── libnx.prx     # libnx runtime
│   └── ...           # Other system libraries
└── app0/             # Game content root
    ├── romfs/        # Game assets
    └── ...
```

## Troubleshooting

### Game crashes immediately

- Check that all `.prx` libraries are in `libs/`
- Verify the RPATH is correct (`ldd game.elf` should show all libs)
- Some games need specific dynamic relocations — check the console output

### Missing library function

- Not all libnx functions are implemented yet
- Run with `ANYDEBUG=1` to see unimplemented function calls
- See `core/libs/prx/libnx/src/` for implementations

### Shader compilation errors

- The Maxwell shader compiler is incomplete
- Disable with `ANYSWITCH_NO_SHADER_CACHE=1`

## Exit Codes

- `0`: Conversion succeeded
- `1`: Invalid arguments
- `2`: Conversion failed (error printed to stderr)
