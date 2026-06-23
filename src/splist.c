#include "splist.h"
#include "splist_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Portable case-insensitive string compare (strcasecmp is POSIX-only and
 * _stricmp is Windows-only, so we roll a small one). Returns 0 when equal. */
static int ci_equal(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

const char *splist_transport_name(splist_transport_t t)
{
    switch (t) {
    case SPLIST_TRANSPORT_USB:
        return "USB";
    case SPLIST_TRANSPORT_BLUETOOTH:
        return "Bluetooth";
    case SPLIST_TRANSPORT_PCI:
        return "PCI";
    case SPLIST_TRANSPORT_UNKNOWN: /* fall through */
    default:
        return "Unknown";
    }
}

/* Order ports by path so output is stable across runs (the OS enumeration
 * order is arbitrary) and so "first match" lookups are deterministic. */
static int port_cmp(const void *a, const void *b)
{
    const splist_port_t *pa = a, *pb = b;
    return strcmp(pa->path, pb->path);
}

splist_status_t splist_enumerate(splist_port_t **out_ports, size_t *out_count)
{
    splist_status_t st = splist_backend_enumerate(out_ports, out_count);
    if (st == SPLIST_OK && *out_ports != NULL && *out_count > 1) qsort(*out_ports, *out_count, sizeof(**out_ports), port_cmp);
    return st;
}

void splist_free(splist_port_t *ports) { free(ports); }

int splist_find_by_serial(const char *serial, splist_port_t *out)
{
    splist_port_t *ports = NULL;
    size_t count = 0;
    int result = 1; /* no match */

    if (serial == NULL || out == NULL) return SPLIST_ERR_IO;

    if (splist_enumerate(&ports, &count) != SPLIST_OK) return SPLIST_ERR_IO;

    for (size_t i = 0; i < count; ++i) {
        if (ports[i].transport == SPLIST_TRANSPORT_USB && ci_equal(ports[i].serial_number, serial)) {
            *out = ports[i];
            result = SPLIST_OK;
            break;
        }
    }

    splist_free(ports);
    return result;
}
