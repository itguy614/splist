# splist

A small, portable C command-line tool to list serial ports along with their USB
metadata, and to look up a serial port's path by its USB serial number.

This is a C port of [splist-rust](https://github.com/patbonecrusher/splist-rust),
written in C99 for portability across Linux, macOS, and Windows.

## Commands

```
splist [list] [--all] [--json] [--vid <hex>] [--pid <hex>] [--manufacturer <str>]
splist get-path --sn <SN> [--json]
splist wait --sn <SN> [--timeout <sec>] [--json]
splist --help
```

Output is always sorted by path, so it is stable across runs.

**Friendly shortcuts:** with no arguments `splist` lists ports; passing `--sn`
without a command runs `get-path`.

### `list`

Prints each port's path and transport type (USB / Bluetooth / PCI / Unknown).
For USB ports it also prints VID:PID, serial number, manufacturer, and product
strings when available, plus the stable `by-id` / `by-path` aliases on Linux.
Pass `--json` for machine-readable output.

By default `list` shows only ports that can actually be connected to. On Linux
this filters out the many phantom `/dev/ttyS*` 8250 placeholders that report no
hardware (probed via the `TIOCGSERIAL` ioctl); USB and Bluetooth ports are
always considered connectable. Use `--all` to list every enumerated port.

Filter the list with `--vid` / `--pid` (hex, e.g. `--vid 303a`) and/or
`--manufacturer <str>` (case-insensitive substring). Filters combine (AND).

### `get-path`

Scans the ports and prints the path of the first USB port whose serial number
matches `<SN>` (case-insensitive). Exits non-zero if no port matches. With
`--json` it prints the matching port object instead of the bare path.

### `wait`

Blocks until a USB port with serial `<SN>` appears, then prints its path (or
object with `--json`) and exits 0 — handy for "plug in the board, then flash"
automation. Without `--timeout` it waits indefinitely; with `--timeout <sec>`
it exits non-zero if the port has not appeared in time.

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

## Acknowledgments

This project is a C port of and was inspired by
[splist-rust](https://github.com/patbonecrusher/splist-rust) by
[patbonecrusher](https://github.com/patbonecrusher). Many thanks for the
original idea and CLI design.

## License

Released under the [MIT License](LICENSE). Copyright (c) 2026 Kurt Wolf.
