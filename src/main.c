/* Request POSIX nanosleep under -std=c99. */
#define _POSIX_C_SOURCE 200809L

#include "splist.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static const char *PROG = "splist";

/* Injected by the build from the VERSION file; fall back if built ad hoc. */
#ifndef SPLIST_VERSION
#define SPLIST_VERSION "unknown"
#endif

#define SPLIST_COPYRIGHT "Copyright (c) 2026 Kurt Wolf. Released under the MIT License."

static void print_version(void)
{
    printf("%s %s\n", PROG, SPLIST_VERSION);
    printf("%s\n", SPLIST_COPYRIGHT);
}

static void print_usage(FILE *out)
{
    fprintf(out,
            "splist %s - list serial ports and find a port by USB serial number\n"
            "\n"
            "Usage:\n"
            "  %s [list] [--all] [--json] [--vid <hex>] [--pid <hex>] [--manufacturer <str>]\n"
            "        List connectable serial ports (the default when no serial is given).\n"
            "  %s <serial> [--wait [--timeout <sec>]] [--json]\n"
            "        Print the path of the USB port with the given serial number.\n"
            "        With --wait, block until that port appears.\n"
            "  %s --ver | --help\n"
            "        Print the version, or show this help.\n"
            "\n"
            "Options:\n"
            "  --all                List every port, including non-connectable ones.\n"
            "  --json               Emit machine-readable JSON.\n"
            "  --vid <hex>          Filter list by USB vendor id, e.g. 303a.\n"
            "  --pid <hex>          Filter list by USB product id, e.g. 1001.\n"
            "  --manufacturer <s>   Filter list by manufacturer substring (case-insensitive).\n"
            "  --wait               Wait for the serial port to appear before printing.\n"
            "  --timeout <sec>      With --wait, give up after <sec> seconds (default: forever).\n"
            "\n"
            "Examples:\n"
            "  %s                                 # list connectable ports\n"
            "  %s --all --json                    # every port, as JSON\n"
            "  %s --vid 303a --manufacturer esp   # filter to Espressif devices\n"
            "  %s 3C:DC:75:65:AF:A4               # print that board's /dev path\n"
            "  %s 3C:DC:75:65:AF:A4 --wait --timeout 30\n"
            "                                     # wait up to 30s for the board\n"
            "  PORT=$(%s 3C:DC:75:65:AF:A4) && echo \"flashing $PORT\"\n"
            "\n"
            "%s\n",
            SPLIST_VERSION, PROG, PROG, PROG, PROG, PROG, PROG, PROG, PROG, PROG, SPLIST_COPYRIGHT);
}

/* Case-insensitive substring test; an empty/NULL needle always matches. */
static int ci_contains(const char *hay, const char *needle)
{
    if (needle == NULL || needle[0] == '\0') return 1;
    size_t nl = strlen(needle);
    for (; *hay; ++hay) {
        size_t k = 0;
        while (k < nl && hay[k] && tolower((unsigned char)hay[k]) == tolower((unsigned char)needle[k]))
            ++k;
        if (k == nl) return 1;
    }
    return 0;
}

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* ---- human-readable output ---- */

static void print_port(const splist_port_t *p)
{
    printf("%s [%s]\n", p->path, splist_transport_name(p->transport));
    if (p->transport == SPLIST_TRANSPORT_USB) {
        if (p->has_usb_ids) printf("    VID:PID        %04x:%04x\n", p->vid, p->pid);
        if (p->serial_number[0]) printf("    Serial Number  %s\n", p->serial_number);
        if (p->manufacturer[0]) printf("    Manufacturer   %s\n", p->manufacturer);
        if (p->product[0]) printf("    Product        %s\n", p->product);
    }
    if (p->by_id[0]) printf("    By-Id          %s\n", p->by_id);
    if (p->by_path[0]) printf("    By-Path        %s\n", p->by_path);
}

/* ---- JSON output ---- */

/* Print a JSON string literal for s, escaping per RFC 8259. */
static void json_string(const char *s)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        switch (*p) {
        case '"':
            fputs("\\\"", stdout);
            break;
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '\b':
            fputs("\\b", stdout);
            break;
        case '\f':
            fputs("\\f", stdout);
            break;
        case '\n':
            fputs("\\n", stdout);
            break;
        case '\r':
            fputs("\\r", stdout);
            break;
        case '\t':
            fputs("\\t", stdout);
            break;
        default:
            if (*p < 0x20)
                printf("\\u%04x", *p);
            else
                putchar(*p);
        }
    }
    putchar('"');
}

static void json_field_str(const char *key, const char *val, int trailing_comma)
{
    printf("    ");
    json_string(key);
    printf(": ");
    json_string(val);
    printf(trailing_comma ? ",\n" : "\n");
}

static void print_port_json(const splist_port_t *p, int last)
{
    printf("  {\n");
    json_field_str("path", p->path, 1);
    json_field_str("transport", splist_transport_name(p->transport), 1);

    if (p->transport == SPLIST_TRANSPORT_USB && p->has_usb_ids) {
        printf("    \"vid\": \"%04x\",\n", p->vid);
        printf("    \"pid\": \"%04x\",\n", p->pid);
    } else {
        printf("    \"vid\": null,\n");
        printf("    \"pid\": null,\n");
    }
    json_field_str("serialNumber", p->serial_number, 1);
    json_field_str("manufacturer", p->manufacturer, 1);
    json_field_str("product", p->product, 1);
    json_field_str("byId", p->by_id, 1);
    json_field_str("byPath", p->by_path, 0);
    printf(last ? "  }\n" : "  },\n");
}

static void print_ports_json(const splist_port_t *ports, size_t count)
{
    printf("[\n");
    for (size_t i = 0; i < count; ++i)
        print_port_json(&ports[i], i + 1 == count);
    printf("]\n");
}

/* ---- commands ---- */

static int cmd_list(int as_json, int show_all, long want_vid, long want_pid, const char *want_mfg)
{
    splist_port_t *ports = NULL;
    size_t count = 0;
    splist_status_t st = splist_enumerate(&ports, &count);

    if (st == SPLIST_ERR_UNSUPPORTED) {
        fprintf(stderr, "%s: this platform is not supported yet\n", PROG);
        return 2;
    }
    if (st != SPLIST_OK) {
        fprintf(stderr, "%s: failed to enumerate serial ports\n", PROG);
        return 1;
    }

    /* Compact the array in place down to the ports we intend to show, so both
     * the count and the JSON array reflect the active filters. */
    size_t shown = 0;
    for (size_t i = 0; i < count; ++i) {
        const splist_port_t *p = &ports[i];
        if (!show_all && !p->connectable) continue;
        if (want_vid >= 0 && !(p->has_usb_ids && p->vid == (uint16_t)want_vid)) continue;
        if (want_pid >= 0 && !(p->has_usb_ids && p->pid == (uint16_t)want_pid)) continue;
        if (want_mfg && !ci_contains(p->manufacturer, want_mfg)) continue;
        ports[shown++] = *p;
    }

    if (as_json) {
        print_ports_json(ports, shown);
    } else {
        printf("Found %zu serial port%s\n", shown, shown == 1 ? "" : "s");
        for (size_t i = 0; i < shown; ++i)
            print_port(&ports[i]);
    }

    splist_free(ports);
    return 0;
}

static int cmd_get_path(const char *serial, int as_json)
{
    splist_port_t port;
    int r = splist_find_by_serial(serial, &port);

    if (r == SPLIST_OK) {
        if (as_json)
            print_port_json(&port, 1);
        else
            printf("%s\n", port.path);
        return 0;
    }
    if (r == SPLIST_ERR_IO) {
        fprintf(stderr, "%s: failed to enumerate serial ports\n", PROG);
        return 1;
    }
    fprintf(stderr, "%s: no port found with serial number '%s'\n", PROG, serial);
    return 1;
}

static int cmd_wait(const char *serial, int as_json, long timeout_sec)
{
    time_t start = time(NULL);

    for (;;) {
        splist_port_t port;
        int r = splist_find_by_serial(serial, &port);

        if (r == SPLIST_OK) {
            if (as_json)
                print_port_json(&port, 1);
            else
                printf("%s\n", port.path);
            return 0;
        }
        if (r == SPLIST_ERR_IO) {
            fprintf(stderr, "%s: failed to enumerate serial ports\n", PROG);
            return 1;
        }

        if (timeout_sec > 0 && (time(NULL) - start) >= timeout_sec) {
            fprintf(stderr, "%s: timed out after %lds waiting for serial '%s'\n", PROG, timeout_sec, serial);
            return 1;
        }
        sleep_ms(250);
    }
}

/* ---- argument parsing ---- */

/* Fetch the value for a flag that expects one, or report and bail. Sets *err. */
static const char *flag_value(int argc, char **argv, int *i, int *err)
{
    if (*i + 1 >= argc) {
        fprintf(stderr, "%s: %s requires a value\n", PROG, argv[*i]);
        *err = 1;
        return NULL;
    }
    return argv[++(*i)];
}

int main(int argc, char **argv)
{
    /* The single positional argument is either the literal "list" or a serial
     * number to look up. No positional means list. */
    const char *positional = NULL;
    const char *want_mfg = NULL;
    int as_json = 0, show_all = 0, do_wait = 0;
    long want_vid = -1, want_pid = -1, timeout_sec = 0;
    int err = 0;

    for (int i = 1; i < argc && !err; ++i) {
        const char *a = argv[i];

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_usage(stdout);
            return 0;
        } else if (strcmp(a, "--ver") == 0 || strcmp(a, "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(a, "--json") == 0) {
            as_json = 1;
        } else if (strcmp(a, "--all") == 0) {
            show_all = 1;
        } else if (strcmp(a, "--wait") == 0) {
            do_wait = 1;
        } else if (strcmp(a, "--vid") == 0) {
            const char *v = flag_value(argc, argv, &i, &err);
            if (v) want_vid = strtol(v, NULL, 16);
        } else if (strcmp(a, "--pid") == 0) {
            const char *v = flag_value(argc, argv, &i, &err);
            if (v) want_pid = strtol(v, NULL, 16);
        } else if (strcmp(a, "--manufacturer") == 0 || strcmp(a, "--mfg") == 0) {
            want_mfg = flag_value(argc, argv, &i, &err);
        } else if (strcmp(a, "--timeout") == 0) {
            const char *v = flag_value(argc, argv, &i, &err);
            if (v) timeout_sec = strtol(v, NULL, 10);
        } else if (a[0] != '-' && positional == NULL) {
            positional = a; /* "list" or a serial number */
        } else {
            fprintf(stderr, "%s: unknown argument '%s'\n\n", PROG, a);
            print_usage(stderr);
            return 2;
        }
    }
    if (err) {
        print_usage(stderr);
        return 2;
    }

    /* No positional, or the literal "list", means list mode. */
    if (positional == NULL || strcmp(positional, "list") == 0) {
        if (do_wait) {
            fprintf(stderr, "%s: --wait needs a serial number to wait for\n", PROG);
            return 2;
        }
        return cmd_list(as_json, show_all, want_vid, want_pid, want_mfg);
    }

    /* Otherwise the positional is a serial number to look up. */
    if (do_wait) return cmd_wait(positional, as_json, timeout_sec);
    return cmd_get_path(positional, as_json);
}
