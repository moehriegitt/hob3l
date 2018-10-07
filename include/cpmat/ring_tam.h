/* -*- Mode: C -*- */
/* Copyright (C) 2018 by Henrik Theiling, License: GPLv3, see LICENSE file */

/**
 * @file
 * A list that is non-directional: there are two neighbours to
 * each node, but there is no predefined direction.
 *
 * This has the advantage that two rings can be split and remerged
 * arbitrarily in O(1), as there cannot be the case where the
 * direction of the newly connected sublists is different, which
 * would require reordering one sublist.
 *
 * For iteration, obviously a direction needs to be chosen.  To do
 * this every call to iterator on next or prev function requires
 * two points of reference: two edges, a point and an edge, or
 * two points.
 *
 * To store a ring that do define a direction, the simplest is to
 * store two points.
 */

/*
 * The implementation could use an ADD list, a XOR list, or a double linked
 * list.  The double linked list allows consistency asserts.  The ADD and
 * XOR lists are smaller (1 pointer vs. 2 pointers).
 *
 * The advantage of AND compare to XOR ist that singletons, pairs, and
 * mirrors are not the same value, which may obscure bugs.  The
 * drawback is that 0 is not a good initialisation value.  A SUB list
 * was not used because it is directional.
 *
 * The advantage of a doubly linked list is that from a single node,
 * you can find its neighbours.  This makes certain operations' prototype
 * simpler (e.g., remove()).  Also, a double linked list is more debuggable
 * and we can have more assert()s.
 *
 * This is implemented with a doubly linked list.
 */

#ifndef __CP_RING_TAM_H
#define __CP_RING_TAM_H

#include <stddef.h>
#include <cpmat/ring_fwd.h>

/**
 * A cell in the ring
 */
struct cp_ring {
    cp_ring_t *n[2];
};

#endif /* __CP_RING_TAM_H */
