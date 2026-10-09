# Pull request

## What changed and why

## How tested

- [ ] `cmake -S . -B build -G Ninja -DBUILD_TESTING=ON`
- [ ] `cmake --build build --parallel`
- [ ] `ctest --test-dir build --output-on-failure`

## Checklist

- [ ] No games, keys, firmware, dumps, or copyrighted data included
- [ ] No decryption tooling, keysets, or proprietary-binary acquisition content
- [ ] Tests added or updated for the changed behavior
- [ ] Docs updated (`README`, `docs/`, or module README as needed)
