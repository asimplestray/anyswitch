# Security Policy

## Supported versions

Only the `main` branch receives security fixes. There are no releases yet (alpha).

| Branch | Supported |
|---|---|
| `main` | Yes |
| feature branches / forks | No — rebase onto `main` |

## Reporting a vulnerability

Open a **private** report via GitHub Security Advisories. If that is unavailable, email the maintainers listed in `CODEOWNERS` (or open a minimal issue asking for a contact without details).

Include:
- Affected file/commit, build flags, OS/compiler.
- Reproduction steps with synthetic data only (no games, keys, or firmware).
- Impact assessment if known.

We aim to acknowledge within 72 hours and to land or decline a fix within 14 days.

## Scope notes

- This project must never handle decryption keys, firmware blobs, or game dumps. Reports containing those will be deleted.
- Build-system command injection, path traversal in `relinker` I/O, and shader-parser crashes are in scope.
