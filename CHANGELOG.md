# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.6.0] - 2026-06-23

### Added
- List serial ports with transport type and, for USB devices, VID:PID, serial number, manufacturer, and product
- Find a port by USB serial number via a positional argument (case-insensitive)
- `--wait` (with optional `--timeout`) to block until a matching port appears
- `--json` machine-readable output for listing and lookup
- `--all` to include non-connectable ports, plus `--vid` / `--pid` / `--manufacturer` filters
- Stable `by-id` / `by-path` device aliases on Linux
- Connectable-by-default listing (phantom 8250 ports filtered via sysfs, without opening the device)
- `--ver` / `--help` with usage examples and copyright/license
- Cross-platform enumeration backends: Linux (sysfs), macOS (IOKit), Windows (SetupAPI)
- GitHub Actions CI (build and smoke-test on Linux, macOS, and Windows) and a tag-driven release workflow
