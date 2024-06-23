%{

#define ELEM_MAXLEN {maxlen}

typedef struct {
    char name[ELEM_MAXLEN + 1];
    recurse_t recurse;
    callback_t callback;
} {value_t};

/** what to do by default */
static {value_t} const elem_fallback = {
    "", csg2_from_rec_xform_warn, csg2_from_elem
};

%}

prefix: elem_
default: &elem_fallback

%%

svg,           csg2_from_svg,       csg2_from_elem
g,             csg2_from_rec_xform, csg2_from_elem
circle,        csg2_from_rec_xform, csg2_from_circle
ellipse,       csg2_from_rec_xform, csg2_from_ellipse
line,          csg2_from_rec_xform, csg2_from_line
rect,          csg2_from_rec_xform, csg2_from_rect
path,          csg2_from_rec_xform, csg2_from_path
polygon,       csg2_from_rec_xform, csg2_from_polygon
polyline,      csg2_from_rec_xform, csg2_from_polyline
title,         csg2_from_no_rec,    NULL,
desc,          csg2_from_no_rec,    NULL,
metadata,      csg2_from_no_rec,    NULL,
style,         csg2_from_no_rec,    NULL,
defs,          csg2_from_no_rec,    NULL,
use,           csg2_from_no_rec,    NULL,
symbol,        csg2_from_no_rec,    NULL,
switch,        csg2_from_no_rec,    NULL,
marker,        csg2_from_no_rec,    NULL,
image,         csg2_from_no_rec,    NULL,
foreignObject, csg2_from_no_rec,    NULL,
