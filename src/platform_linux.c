#ifdef __linux__

#include "splist_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/serial.h>

/* Linux backend using sysfs only (no libudev dependency).
 *
 * Serial ports surface as /sys/class/tty/<name>. Real devices (as opposed to
 * pseudo-terminals and virtual consoles) have a "device" symlink pointing into
 * the device tree. For USB-backed ttys we walk up the device tree to the
 * owning USB device directory, which exposes idVendor/idProduct/serial/etc. */

#define SYS_TTY_DIR "/sys/class/tty"

/* Read the first line of a sysfs attribute file into buf (NUL-terminated,
 * trailing newline stripped). Leaves buf as an empty string on failure. */
static void read_attr(const char *dir, const char *attr, char *buf, size_t buflen)
{
    char path[PATH_MAX + 64];
    FILE *f;

    if (buflen == 0)
        return;
    buf[0] = '\0';

    snprintf(path, sizeof(path), "%s/%s", dir, attr);
    f = fopen(path, "r");
    if (f == NULL)
        return;

    if (fgets(buf, (int)buflen, f) != NULL) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
    }
    fclose(f);
}

/* Returns nonzero if `dir` looks like a USB device directory (has idVendor). */
static int is_usb_device_dir(const char *dir)
{
    char path[PATH_MAX + 64];
    snprintf(path, sizeof(path), "%s/idVendor", dir);
    return access(path, R_OK) == 0;
}

/* Walk up from a tty's resolved device directory toward the root, looking for
 * the nearest USB device directory. Writes its path into usb_dir on success. */
static int find_usb_parent(const char *device_dir, char *usb_dir, size_t len)
{
    char cur[PATH_MAX];
    snprintf(cur, sizeof(cur), "%s", device_dir);

    for (;;) {
        if (is_usb_device_dir(cur)) {
            snprintf(usb_dir, len, "%s", cur);
            return 1;
        }
        /* Strip the last path component to move one level up. */
        char *slash = strrchr(cur, '/');
        if (slash == NULL || slash == cur)
            return 0;
        *slash = '\0';
        /* Stop once we climb out of the /sys/devices tree. */
        if (strstr(cur, "/devices") == NULL)
            return 0;
    }
}

/* Decide whether a non-USB tty is backed by real, openable hardware. The 8250
 * driver pre-creates /dev/ttyS0..N regardless of what is populated; the kernel
 * only knows a true UART type once probed, so we ask via TIOCGSERIAL and treat
 * PORT_UNKNOWN as "nothing attached". A port that is open by something else
 * (EBUSY) or that we lack permission to probe (EACCES) is assumed real. */
static int uart_is_present(const char *dev_path)
{
    int fd = open(dev_path, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd < 0)
        return (errno == EBUSY || errno == EACCES) ? 1 : 0;

    struct serial_struct ser;
    int present = 1; /* if the ioctl is unsupported, don't hide the port */
    if (ioctl(fd, TIOCGSERIAL, &ser) == 0)
        present = (ser.type != PORT_UNKNOWN);
    close(fd);
    return present;
}

/* Look in a /dev/serial/<kind> directory for the symlink that resolves to the
 * tty named `tty_name`, and write its full path into out. These aliases are
 * created by udev and are stable across replug/reboot. Empty out on miss. */
static void find_dev_serial_link(const char *kind, const char *tty_name,
                                 char *out, size_t outlen)
{
    char dir[128];
    DIR *d;
    struct dirent *ent;

    if (outlen)
        out[0] = '\0';

    snprintf(dir, sizeof(dir), "/dev/serial/%s", kind);
    d = opendir(dir);
    if (d == NULL)
        return; /* directory is absent when no by-id/by-path links exist */

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        char link[PATH_MAX];
        char target[PATH_MAX];
        snprintf(link, sizeof(link), "%s/%s", dir, ent->d_name);

        ssize_t n = readlink(link, target, sizeof(target) - 1);
        if (n < 0)
            continue;
        target[n] = '\0';

        /* Targets look like "../../ttyACM0"; compare the final component. */
        const char *base = strrchr(target, '/');
        base = base ? base + 1 : target;
        if (strcmp(base, tty_name) == 0) {
            snprintf(out, outlen, "%s", link);
            break;
        }
    }
    closedir(d);
}

/* Classify and fill metadata for one tty whose name is `name`. Returns 0 if
 * this entry should be skipped (no backing device, e.g. a pseudo-terminal). */
static int fill_port(const char *name, splist_port_t *port)
{
    char link_path[PATH_MAX];
    char device_dir[PATH_MAX];

    memset(port, 0, sizeof(*port));
    /* Bound the name with a precision specifier: tty names are always short,
     * but readdir entries can be up to NAME_MAX, which the compiler flags. */
    snprintf(port->path, sizeof(port->path), "/dev/%.*s",
             (int)(sizeof(port->path) - 6), name);
    port->transport = SPLIST_TRANSPORT_UNKNOWN;

    /* Resolve /sys/class/tty/<name>/device to the real device directory. */
    snprintf(link_path, sizeof(link_path), "%s/%s/device", SYS_TTY_DIR, name);
    char resolved[PATH_MAX];
    if (realpath(link_path, resolved) == NULL)
        return 0; /* no backing device; skip virtual/pseudo ttys */
    snprintf(device_dir, sizeof(device_dir), "%s", resolved);

    char usb_dir[PATH_MAX];
    if (find_usb_parent(device_dir, usb_dir, sizeof(usb_dir))) {
        char tmp[SPLIST_STR_MAX];

        port->transport = SPLIST_TRANSPORT_USB;

        read_attr(usb_dir, "idVendor", tmp, sizeof(tmp));
        if (tmp[0] != '\0') {
            port->vid = (uint16_t)strtol(tmp, NULL, 16);
            read_attr(usb_dir, "idProduct", tmp, sizeof(tmp));
            port->pid = (uint16_t)strtol(tmp, NULL, 16);
            port->has_usb_ids = 1;
        }
        read_attr(usb_dir, "serial", port->serial_number, sizeof(port->serial_number));
        read_attr(usb_dir, "manufacturer", port->manufacturer, sizeof(port->manufacturer));
        read_attr(usb_dir, "product", port->product, sizeof(port->product));
    } else if (strstr(device_dir, "/pci") != NULL ||
               strstr(device_dir, ":") != NULL) {
        port->transport = SPLIST_TRANSPORT_PCI;
    }

    /* USB-attached ports are always connectable; for legacy UARTs we probe to
     * weed out the phantom 8250 placeholders. */
    port->connectable = (port->transport == SPLIST_TRANSPORT_USB)
                            ? 1
                            : uart_is_present(port->path);

    find_dev_serial_link("by-id", name, port->by_id, sizeof(port->by_id));
    find_dev_serial_link("by-path", name, port->by_path, sizeof(port->by_path));

    return 1;
}

splist_status_t splist_backend_enumerate(splist_port_t **out_ports, size_t *out_count)
{
    DIR *d;
    struct dirent *ent;
    splist_port_t *ports = NULL;
    size_t count = 0, cap = 0;

    *out_ports = NULL;
    *out_count = 0;

    d = opendir(SYS_TTY_DIR);
    if (d == NULL)
        return SPLIST_ERR_IO;

    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        splist_port_t port;
        if (!fill_port(ent->d_name, &port))
            continue;

        if (count == cap) {
            size_t newcap = cap == 0 ? 8 : cap * 2;
            splist_port_t *grown = realloc(ports, newcap * sizeof(*grown));
            if (grown == NULL) {
                free(ports);
                closedir(d);
                return SPLIST_ERR_NOMEM;
            }
            ports = grown;
            cap = newcap;
        }
        ports[count++] = port;
    }
    closedir(d);

    *out_ports = ports;
    *out_count = count;
    return SPLIST_OK;
}

#endif /* __linux__ */
