#ifdef __APPLE__

#include "splist_internal.h"

#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>

/* kIOMainPortDefault was introduced in the macOS 12 SDK; older SDKs spell the
 * same value kIOMasterPortDefault. Fall back so we build on both. */
#ifndef kIOMainPortDefault
#define kIOMainPortDefault kIOMasterPortDefault
#endif

/* macOS backend using IOKit.
 *
 * Serial devices are IOSerialBSDClient services; each exposes a callout device
 * path (/dev/cu.*) and a dial-in path (/dev/tty.*). We report the callout path,
 * matching the convention the Rust tool uses on macOS. USB metadata (VID/PID,
 * serial number, manufacturer, product) lives on an ancestor IOUSBDevice /
 * IOUSBHostDevice node, so we walk up the registry to collect it. */

/* Copy a CFString registry property of `service` into buf. Empty on miss. */
static void copy_str_prop(io_registry_entry_t service, CFStringRef key,
                          char *buf, size_t buflen)
{
    if (buflen == 0)
        return;
    buf[0] = '\0';

    CFTypeRef val = IORegistryEntrySearchCFProperty(
        service, kIOServicePlane, key, kCFAllocatorDefault,
        kIORegistryIterateRecursively | kIORegistryIterateParents);
    if (val == NULL)
        return;

    if (CFGetTypeID(val) == CFStringGetTypeID())
        CFStringGetCString((CFStringRef)val, buf, (CFIndex)buflen,
                           kCFStringEncodingUTF8);
    CFRelease(val);
}

/* Read a CFNumber registry property (searched up the parent chain) as an int.
 * Returns nonzero on success. */
static int copy_num_prop(io_registry_entry_t service, CFStringRef key, long *out)
{
    CFTypeRef val = IORegistryEntrySearchCFProperty(
        service, kIOServicePlane, key, kCFAllocatorDefault,
        kIORegistryIterateRecursively | kIORegistryIterateParents);
    if (val == NULL)
        return 0;

    int ok = 0;
    if (CFGetTypeID(val) == CFNumberGetTypeID()) {
        long n = 0;
        if (CFNumberGetValue((CFNumberRef)val, kCFNumberLongType, &n)) {
            *out = n;
            ok = 1;
        }
    }
    CFRelease(val);
    return ok;
}

static int fill_port(io_object_t service, splist_port_t *port)
{
    memset(port, 0, sizeof(*port));
    port->transport = SPLIST_TRANSPORT_UNKNOWN;
    /* IOSerialBSDClient only surfaces ports with present hardware. */
    port->connectable = 1;

    /* Prefer the callout device (/dev/cu.*); fall back to the dial-in path. */
    char path[SPLIST_STR_MAX] = {0};
    copy_str_prop(service, CFSTR(kIOCalloutDeviceKey), path, sizeof(path));
    if (path[0] == '\0')
        copy_str_prop(service, CFSTR(kIODialinDeviceKey), path, sizeof(path));
    if (path[0] == '\0')
        return 0;
    snprintf(port->path, sizeof(port->path), "%s", path);

    /* USB metadata is found by searching ancestors for USB device properties.
     * If a vendor ID is present we treat the port as USB-backed. */
    long vid = 0, pid = 0;
    int has_vid = copy_num_prop(service, CFSTR(kUSBVendorID), &vid);
    int has_pid = copy_num_prop(service, CFSTR(kUSBProductID), &pid);

    if (has_vid) {
        port->transport = SPLIST_TRANSPORT_USB;
        port->vid = (uint16_t)vid;
        if (has_pid)
            port->pid = (uint16_t)pid;
        port->has_usb_ids = 1;

        copy_str_prop(service, CFSTR(kUSBSerialNumberString),
                      port->serial_number, sizeof(port->serial_number));
        copy_str_prop(service, CFSTR(kUSBVendorString),
                      port->manufacturer, sizeof(port->manufacturer));
        copy_str_prop(service, CFSTR(kUSBProductString),
                      port->product, sizeof(port->product));
    }

    return 1;
}

splist_status_t splist_backend_enumerate(splist_port_t **out_ports, size_t *out_count)
{
    *out_ports = NULL;
    *out_count = 0;

    CFMutableDictionaryRef match = IOServiceMatching(kIOSerialBSDServiceValue);
    if (match == NULL)
        return SPLIST_ERR_IO;

    /* Restrict to true serial ports (rs-232 style), as the Rust tool does. */
    CFDictionarySetValue(match, CFSTR(kIOSerialBSDTypeKey),
                         CFSTR(kIOSerialBSDAllTypes));

    io_iterator_t iter = MACH_PORT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &iter)
            != KERN_SUCCESS)
        return SPLIST_ERR_IO;

    splist_port_t *ports = NULL;
    size_t count = 0, cap = 0;

    io_object_t service;
    while ((service = IOIteratorNext(iter)) != MACH_PORT_NULL) {
        splist_port_t port;
        int kept = fill_port(service, &port);
        IOObjectRelease(service);
        if (!kept)
            continue;

        if (count == cap) {
            size_t newcap = cap == 0 ? 8 : cap * 2;
            splist_port_t *grown = realloc(ports, newcap * sizeof(*grown));
            if (grown == NULL) {
                free(ports);
                IOObjectRelease(iter);
                return SPLIST_ERR_NOMEM;
            }
            ports = grown;
            cap = newcap;
        }
        ports[count++] = port;
    }
    IOObjectRelease(iter);

    *out_ports = ports;
    *out_count = count;
    return SPLIST_OK;
}

#endif /* __APPLE__ */
