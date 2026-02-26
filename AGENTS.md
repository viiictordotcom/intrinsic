# Repository Guidelines

## Project Structure & Module Organization

- `src/` contains the main application C++20 code; `src/main.cpp` is the entry point.
- `tests/` contains C++ test sources (`*_test.cpp`) and test helpers.
- `src/db/` owns the SQLite schema and query layer.
- `src/views/` implements ncurses views and input handling.
- `build/`, `build-release/`, and `build-sanitize/` are generated output; do not edit or commit.
- `justfile` defines local tasks; `flake.nix` provides the Nix dev shell and dependencies.

## Build, Test, and Development Commands

Prefer `just` (see `just --list`) or call CMake directly.
`just build`, `just test`, `just run`, `just debug`, and `just cppcheck` implicitly run `configure`; `just test-sanitize` implicitly runs `configure-sanitize`, so manual configure is usually unnecessary.

- `just` (default): runs the default recipe, equivalent to `just run`.
- `just build`: compile the debug binary at `build/intrinsic`.
- `just test`: build `intrinsic_tests` and run the suite with CTest from `build/`.
- `just test-sanitize`: build `intrinsic_tests` with sanitizers and run CTest from `build-sanitize/`.
- `just test-all`: run `clean`, `cppcheck`, `test`, and `test-sanitize` in sequence for a full local validation pass.
- `just run`: build, then run the ncurses app.
- `just debug`: launch the app under lldb.
- `just cppcheck`: run static analysis using `compile_commands.json`.
- `just release` / `just run-release`: build and run a release binary in `build-release/`.
- `just clean`: remove build directories.

## Nix-Only Install and Update

- This project is distributed as a **Nix-only package**.
- User install/update flow (rolling, non-versioned):
  - `nix profile add github:viiictordotcom/intrinsic#default`: install to the user profile and enable the `intrinsic` command.
  - `nix profile upgrade intrinsic --refresh`: rolling update to latest upstream revision.
  - In app settings, `U` then `U` runs the same update command; users should restart `intrinsic` after updates.
- Local source usage (development/smoke run from this directory):
  - `nix run .#default`: build and run without profile installation.
- Cross-platform notes:
  - Supported package targets are Linux and macOS (`x86_64` and `aarch64`); Windows is not supported.
  - Runtime data/config paths are per-user (`XDG_*` on Linux, `~/Library/Application Support` fallback on macOS).
  - Clipboard support expects OS tools (`wl-copy`/`xclip`/`xsel` on Linux, `pbcopy` on macOS).

## Coding Style & Naming Conventions

- Use simple, safe, modern C++20; avoid overengineering and keep functions focused.
- Types use PascalCase (`Database`, `TickerRow`); functions and variables use `snake_case`.
- Headers are `.hpp`, implementations are `.cpp`; keep declarations in headers and logic in `.cpp`.
- Group includes with standard/system headers first, then project headers.

## Testing Guidelines

- Place tests in `tests/` and name them `*_test.cpp`.
- Run `just test` to build `intrinsic_tests` and execute all checks via CTest.
- Run `just test-sanitize` periodically to execute the suite with ASan/UBSan enabled.
- Use `just test-all` when you want a clean, full verification pass (cleanup, static analysis, normal tests, and sanitizer tests).
- Cover normal behavior plus edge cases (invalid input, overflow boundaries, and injection-like strings).
- Run `just cppcheck` as a baseline static-analysis quality gate.

## Security & Configuration Tips

- Data is stored in SQLite at `$XDG_DATA_HOME/intrinsic/intrinsic.db` or `~/.local/share/intrinsic/intrinsic.db`; never commit local DB files.

## Version Control Policy

- Do not use, reference, or suggest any version control tools or workflows.
- Treat this project as a plain working directory.
