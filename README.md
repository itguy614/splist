# splist

[![CI](https://github.com/itguy614/splist/actions/workflows/ci.yml/badge.svg)](https://github.com/itguy614/splist/actions/workflows/ci.yml)

A small, portable C command-line tool to list serial ports along with their USB
metadata, and to look up a serial port's path by its USB serial number.

This is a C port of [splist-rust](https://github.com/patbonecrusher/splist-rust),
written in C99 for portability across Linux, macOS, and Windows.

## Commands

```
splist [list] [--all] [--json] [--vid <hex>] [--pid <hex>] [--manufacturer <str>]
splist <serial> [--wait [--timeout <sec>]] [--json]
splist --ver | --help
```

`splist` does one of two things: **list** ports, or **look up** a port by its
USB serial number. With no positional argument (or the literal `list`) it
lists; any other bare word is treated as a serial number to look up.

Output is always sorted by path, so it is stable across runs.

### Listing ports

`splist` / `splist list` prints each port's path and transport type
(USB / Bluetooth / PCI / Unknown). For USB ports it also prints VID:PID, serial
number, manufacturer, and product strings when available, plus the stable
`by-id` / `by-path` aliases on Linux. Pass `--json` for machine-readable output.

By default it shows only ports that can actually be connected to. On Linux this
filters out the many phantom `/dev/ttyS*` 8250 placeholders that report no
hardware — detected by reading `/sys/class/tty/<name>/type` (no device is
opened, so attached equipment is never disturbed). USB and Bluetooth ports are
always considered connectable. Use `--all` to list every enumerated port.

Filter with `--vid` / `--pid` (hex, e.g. `--vid 303a`) and/or `--manufacturer
<str>` (case-insensitive substring). Filters combine (AND).

### Finding a port by serial number

`splist <serial>` prints the path of the first USB port whose serial number
matches `<serial>` (case-insensitive), and exits non-zero if none matches. With
`--json` it prints the matching port object instead of the bare path.

Add `--wait` to block until a matching port appears, then print it — handy for
"plug in the board, then flash" automation. Without `--timeout` it waits
indefinitely; with `--timeout <sec>` it exits non-zero if the port has not
appeared in time.

### Stable names on Linux

`/dev/ttyACM0`-style names renumber across replug/reboot. The `by-id` and
`by-path` udev aliases (under `/dev/serial/`) do not, so prefer them in scripts.

## Building

```sh
git clone git@github.com:itguy614/splist.git
cd splist
cmake -S . -B build
cmake --build build
./build/splist list
```

## Platform support

| Platform | Backend            | Status        |
|----------|--------------------|---------------|
| Linux    | sysfs (no libudev) | Implemented   |
| macOS    | IOKit              | Implemented   |
| Windows  | SetupAPI           | Implemented   |

The platform backends share the interface declared in `include/splist.h`. Each
backend is guarded by a compile-time `#ifdef`, so all sources compile on every
platform and only the matching one emits code.

## Versioning & releases

The `VERSION` file at the repo root is the single source of truth for the
version; the build bakes it into the binary (`splist --ver`).

- **CI** builds and smoke-tests every push to `develop` and every pull request
  on all three platforms (no artifacts retained).
- **Releases** are cut by tagging a commit on `master` with a v-prefixed semver
  tag that matches `VERSION`:

  ```sh
  # after bumping VERSION and merging to master
  git tag v0.5.0
  git push origin v0.5.0
  ```

  The release workflow verifies the tag matches `VERSION` (failing on a
  mismatch), builds each platform, and attaches per-OS archives
  (`splist-<version>-<os>-<arch>.tar.gz` / `.zip`, each containing the binary,
  `README.md`, and `LICENSE`) to a GitHub Release.

## Acknowledgments

This project is a C port of and was inspired by
[splist-rust](https://github.com/patbonecrusher/splist-rust) by
[patbonecrusher](https://github.com/patbonecrusher). Many thanks for the
original idea and CLI design.

## License

Released under the [MIT License](LICENSE). Copyright (c) 2026 Kurt Wolf.
