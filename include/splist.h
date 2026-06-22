#ifndef SPLIST_H
#define SPLIST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How the port is physically attached. */
typedef enum {
    SPLIST_TRANSPORT_UNKNOWN = 0,
    SPLIST_TRANSPORT_USB,
    SPLIST_TRANSPORT_BLUETOOTH,
    SPLIST_TRANSPORT_PCI,
} splist_transport_t;

/* Fixed-size buffers keep the struct trivially copyable and free of ownership
 * concerns; serial-port metadata strings are always short in practice. */
#define SPLIST_STR_MAX 256

/* Symlink alias paths (by-id/by-path) can be long; size them for a full path. */
#define SPLIST_PATH_MAX 4096

typedef struct {
    char path[SPLIST_STR_MAX];          /* e.g. "/dev/ttyUSB0" or "COM3" */
    splist_transport_t transport;

    /* Nonzero if real hardware is present that a program could actually open.
     * On Linux this filters out the many phantom /dev/ttyS* 8250 placeholders
     * that report PORT_UNKNOWN; USB/Bluetooth ports are always connectable.
     * Backends always populate this; callers decide whether to filter on it. */
    int connectable;

    /* The fields below are only meaningful when transport == USB. */
    int has_usb_ids;                    /* nonzero if vid/pid are valid */
    uint16_t vid;
    uint16_t pid;
    char serial_number[SPLIST_STR_MAX]; /* empty if unavailable */
    char manufacturer[SPLIST_STR_MAX];  /* empty if unavailable */
    char product[SPLIST_STR_MAX];       /* empty if unavailable */

    /* Stable device aliases that survive replug/reboot (Linux only; empty on
     * other platforms or when the symlink does not exist). by_id encodes the
     * device identity, by_path the physical USB topology slot. */
    char by_id[SPLIST_PATH_MAX];
    char by_path[SPLIST_PATH_MAX];
} splist_port_t;

/* Result codes returned by the public API. */
typedef enum {
    SPLIST_OK = 0,
    SPLIST_ERR_UNSUPPORTED = -1,  /* platform backend not implemented */
    SPLIST_ERR_IO = -2,           /* failed to read system port info */
    SPLIST_ERR_NOMEM = -3,
} splist_status_t;

/* Enumerate all serial ports on the system.
 *
 * On success, *out_ports points to a heap-allocated array of *out_count
 * entries which the caller must release with splist_free(). On any error
 * *out_ports is set to NULL and *out_count to 0. */
splist_status_t splist_enumerate(splist_port_t **out_ports, size_t *out_count);

/* Release an array returned by splist_enumerate(). Safe to call with NULL. */
void splist_free(splist_port_t *ports);

/* Convenience: scan ports and copy the first one whose USB serial number
 * equals `serial` (case-sensitive) into *out. Returns SPLIST_OK on a match,
 * SPLIST_ERR_IO if enumeration failed, or 1 if no port matched. */
int splist_find_by_serial(const char *serial, splist_port_t *out);

/* Human-readable name for a transport enum value. */
const char *splist_transport_name(splist_transport_t t);

#ifdef __cplusplus
}
#endif

#endif /* SPLIST_H */
