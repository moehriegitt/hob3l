/* -*- Mode: C -*- */
/* Copyright (C) 2018-2024 by Henrik Theiling, License: GPLv3, see LICENSE file */

#ifndef CP_XML_PARSE_H_
#define CP_XML_PARSE_H_

#include <hob3lbase/base-def.h>
#include <hob3lbase/err_tam.h>
#include <hob3l/syn_tam.h>

/**
 * String for matching anything */
#define CP_XML_ANY cp_xml_any_

/**
 * Apply 'chomp' to all elements during parsing.
 */
#define CP_XML_OPT_CHOMP 1

extern char const cp_xml_any_[];

/**
 * Return a human readable name for the given enum value
 * of type cp_xml_type_t.
 */
extern char const *cp_xml_type_stringify(
    unsigned x);

/**
 * XML dumper (for debugging) */
extern void cp_xml_dump(
    FILE *f,
    cp_xml_t *n);

/**
 * Whether a character is XML White space
 */
extern bool cp_xml_is_space(unsigned c);

/**
 * Chomp all white space from CDATA of children, then remove all
 * empty CDATA.
 */
extern void cp_xml_chomp(
    cp_xml_t *r);

/**
 * Parse an XML file into an XML syntax tree.
 */
extern bool cp_xml_parse(
    cp_xml_t **rp,
    cp_pool_t *tmp,
    cp_err_t *err,
    cp_syn_file_t *file,
    unsigned opt);

/**
 * Find a node in a list based on different criteria.
 * This can be used to search a node's ->child list or
 * to continue search on a node with it's ->next list.
 */
extern cp_xml_t *cp_xml_find(
    /** The start of the list to search */
    cp_xml_t *n,

    /** The node type to match, or 0 to match any */
    cp_xml_type_t type,

    /** The data to match (using `strequ0()`), or CP_XML_ANY to match any */
    char const *data,

    /** The namespace to match (using `==`), or CP_XML_ANY to match any */
    char const *ns);

/**
 * Recursively set the given `new_ns` to each node that has `ns == old_ns`
 * or `strequ(ns, new_ns)`.
 *
 * For non-element nodes, if `old_ns == NULL`, then the namespace is not reset.
 *
 * If `old_ns` is NULL, this sets the default name space for elements.
 *
 * This can be used to make a given namespace token-identical (to `new_ns`).
 */
extern void cp_xml_set_ns(
    cp_xml_t *x,
    char const *old_ns,
    char const *new_ns);

/**
 * Skip the attribute nodes (which are first in a child list) and return
 * a pointer to the first non-attr pointer (or to NULL).
 */
extern cp_xml_t **cp_xml_skip_attr_p(
    cp_xml_t **xp);

/**
 * Skip the attribute nodes (which are first in a child list) and return
 * the first non-attr pointer (or NULL).
 */
static inline cp_xml_t *cp_xml_skip_attr(
    cp_xml_t *x)
{
    return *cp_xml_skip_attr_p(&x);
}

#endif /* CP_XML_PARSE_H_ */
