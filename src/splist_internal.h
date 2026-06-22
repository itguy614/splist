#ifndef SPLIST_INTERNAL_H
#define SPLIST_INTERNAL_H

#include "splist.h"

/* Per-platform enumeration hook. The public splist_enumerate() (in splist.c)
 * wraps this and applies shared post-processing (stable sort by path), so the
 * backends only need to report the raw set of ports in any order. */
splist_status_t splist_backend_enumerate(splist_port_t **out_ports,
                                         size_t *out_count);

#endif /* SPLIST_INTERNAL_H */
