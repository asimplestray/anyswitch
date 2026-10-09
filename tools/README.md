# tools — developer utilities

Reserved for small host-side helpers (NSO inspection, symbol dumps,
shader test harnesses). Each tool should be a standalone script or a
minimal CMake target with its own README.

Conventions:
- Prefer Python 3 for one-off inspection scripts.
- Prefer C++20 for anything linked into the build.
- No game data, keys, or firmware may be committed here.
