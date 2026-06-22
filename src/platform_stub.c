/* Fallback backend for platforms without a native implementation yet
 * (macOS via IOKit and Windows via SetupAPI are planned). Compiled only when
 * no other backend matches, so the program links and reports cleanly. */
#if !defined(__linux__) && !defined(__APPLE__) && !defined(_WIN32)

#include "splist_internal.h"

#include <stddef.h>

splist_status_t splist_backend_enumerate(splist_port_t **out_ports, size_t *out_count)
{
    *out_ports = NULL;
    *out_count = 0;
    return SPLIST_ERR_UNSUPPORTED;
}

#endif
