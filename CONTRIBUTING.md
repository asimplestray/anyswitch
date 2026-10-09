# Contributing to AnySwitch

Thanks for helping. This project is alpha, so small, well-tested PRs beat large rewrites.

## Ground rules

- English for code, comments, issues, and PRs.
- C++20, 4-space indent, no trailing whitespace, files end with a newline.
- `#pragma once` for new headers (legacy include guards are being migrated).
- Prefer `std::unique_ptr` for ownership, `override`/`final`/`constexpr`/`noexcept` where correct.
- No games, keys, firmware, dumps, or Nintendo-copyrighted material. Ever.
- **No decryption tooling, keysets, or documentation of how to obtain
  decrypted content.** The project's legal posture depends on it: we consume
  user-provided decrypted binaries and stay out of that process entirely.
  PRs adding such content are closed without review.
- You must own what you test with; only synthetic fixtures may be committed.

## Workflow

1. Fork, then branch from `main`: `feat/<topic>`, `fix/<topic>`, `docs/<topic>`.
2. Configure with tests on:
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure
   ```
3. Add or update tests for the behavior you change.
4. Run a local lint pass if you have clangd: `cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.
5. Open a PR with the template filled in. Link related issues.

## What makes a good PR

- One logical change per PR.
- Description covers: what changed, why, how it was tested.
- New modules include a short README or header comment explaining ownership and next steps.
- Shader/recompiler stages land as: decoder → IR → translation → optimization → SPIR-V backend, each independently buildable.
- Host library additions (`core/libs/prx/*`) include the symbol list touched and a unit test or smoke test.

## Good first issues

Look for `good first issue`:
- Filling a `libnx`/`libkernel` stub with a POSIX-backed implementation + test.
- Adding synthetic ELF/NSO fixtures for golden-file relinker tests.
- Landing one Maxwell decoder table with unit coverage.
- Docs: diagrams, troubleshooting entries, build fixes for new distros.

## Commit style

- Imperative subject, ≤72 chars: `Add svcCloseHandle fallback test`.
- Body explains the why when the diff does not.

## Review expectations

- Maintainers check coherence (no PS5/NID leftovers), build health (default + LLVM paths), and legal safety.
- Be kind, assume good intent, prefer suggestions with code snippets.

Questions? Open a discussion or a docs issue before writing a large patch.
