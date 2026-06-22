#ifdef _WIN32

#include "splist_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>

/* Windows backend using SetupAPI.
 *
 * We enumerate the Ports (COM & LPT) device class. For each device we read the
 * "PortName" value (e.g. "COM3") from its registry key, and parse the device
 * instance ID for USB VID/PID and serial number. Friendly-name / manufacturer
 * strings come from SetupAPI device-property registry properties.
 *
 * A USB device instance ID looks like:
 *   USB\VID_303A&PID_1001\3C3C3C3C  (last element is the serial number)
 * Composite devices substitute the serial with an interface id like "6&...",
 * in which case no usable serial number is present. */

static void to_cstr(const char *src, char *dst, size_t dstlen)
{
    if (dstlen == 0)
        return;
    snprintf(dst, dstlen, "%s", src ? src : "");
}

/* Pull "COMx" out of the device's hardware registry key. Empty on failure. */
static void read_port_name(HDEVINFO set, SP_DEVINFO_DATA *dev,
                           char *buf, size_t buflen)
{
    if (buflen)
        buf[0] = '\0';

    HKEY key = SetupDiOpenDevRegKey(set, dev, DICS_FLAG_GLOBAL, 0,
                                    DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE)
        return;

    DWORD type = 0;
    DWORD len = (DWORD)buflen;
    RegQueryValueExA(key, "PortName", NULL, &type, (LPBYTE)buf, &len);
    RegCloseKey(key);
}

/* Read a SetupAPI device registry property (e.g. friendly name) as a string. */
static void read_devprop(HDEVINFO set, SP_DEVINFO_DATA *dev, DWORD prop,
                         char *buf, size_t buflen)
{
    if (buflen)
        buf[0] = '\0';
    SetupDiGetDeviceRegistryPropertyA(set, dev, prop, NULL,
                                      (PBYTE)buf, (DWORD)buflen, NULL);
}

/* Parse VID/PID/serial out of a device instance id. Returns nonzero if the id
 * is USB-backed (VID found), filling vid, pid, and the serial buffer. */
static int parse_instance_id(const char *id, uint16_t *vid, uint16_t *pid,
                             char *serial, size_t serlen)
{
    if (serlen)
        serial[0] = '\0';

    const char *v = strstr(id, "VID_");
    const char *p = strstr(id, "PID_");
    if (v == NULL)
        return 0;

    *vid = (uint16_t)strtol(v + 4, NULL, 16);
    if (p)
        *pid = (uint16_t)strtol(p + 4, NULL, 16);

    /* The instance-specific element follows the last backslash. Treat it as a
     * serial number only if it has no '&' (composite-device interface marker). */
    const char *last = strrchr(id, '\\');
    if (last && strchr(last + 1, '&') == NULL)
        to_cstr(last + 1, serial, serlen);

    return 1;
}

splist_status_t splist_backend_enumerate(splist_port_t **out_ports, size_t *out_count)
{
    *out_ports = NULL;
    *out_count = 0;

    HDEVINFO set = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, NULL, NULL,
                                        DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE)
        return SPLIST_ERR_IO;

    splist_port_t *ports = NULL;
    size_t count = 0, cap = 0;

    SP_DEVINFO_DATA dev;
    dev.cbSize = sizeof(dev);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        char port_name[SPLIST_STR_MAX];
        read_port_name(set, &dev, port_name, sizeof(port_name));
        if (port_name[0] == '\0')
            continue; /* not an addressable COM port */

        splist_port_t port;
        memset(&port, 0, sizeof(port));
        to_cstr(port_name, port.path, sizeof(port.path));
        port.transport = SPLIST_TRANSPORT_UNKNOWN;
        /* DIGCF_PRESENT restricts enumeration to currently present devices. */
        port.connectable = 1;

        char instance_id[512] = {0};
        if (SetupDiGetDeviceInstanceIdA(set, &dev, instance_id,
                                        sizeof(instance_id), NULL)) {
            if (strncmp(instance_id, "USB", 3) == 0) {
                uint16_t vid = 0, pid = 0;
                if (parse_instance_id(instance_id, &vid, &pid,
                                      port.serial_number,
                                      sizeof(port.serial_number))) {
                    port.transport = SPLIST_TRANSPORT_USB;
                    port.vid = vid;
                    port.pid = pid;
                    port.has_usb_ids = 1;

                    read_devprop(set, &dev, SPDRP_MFG,
                                 port.manufacturer, sizeof(port.manufacturer));
                    read_devprop(set, &dev, SPDRP_FRIENDLYNAME,
                                 port.product, sizeof(port.product));
                }
            } else if (strncmp(instance_id, "BTHENUM", 7) == 0) {
                port.transport = SPLIST_TRANSPORT_BLUETOOTH;
            }
        }

        if (count == cap) {
            size_t newcap = cap == 0 ? 8 : cap * 2;
            splist_port_t *grown = realloc(ports, newcap * sizeof(*grown));
            if (grown == NULL) {
                free(ports);
                SetupDiDestroyDeviceInfoList(set);
                return SPLIST_ERR_NOMEM;
            }
            ports = grown;
            cap = newcap;
        }
        ports[count++] = port;
    }

    SetupDiDestroyDeviceInfoList(set);

    *out_ports = ports;
    *out_count = count;
    return SPLIST_OK;
}

#endif /* _WIN32 */
