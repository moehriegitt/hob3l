%{

#define ELEM_MAXLEN {maxlen}

typedef struct {
    char name[ELEM_MAXLEN + 1];
    bool (*callback)(
        cp_v_obj_p_t *,
        ctxt_t const *,
        local_t const *,
        cp_xml_t *);
} {value_t};

%}

prefix: elem_

%%

g,             csg2_from_g
svg,           csg2_from_svg
circle,        csg2_from_circle
ellipse,       csg2_from_ellipse
line,          csg2_from_line
rect,          csg2_from_rect
path,          csg2_from_path
polygon,       csg2_from_polygon
polyline,      csg2_from_polyline
title,         csg2_from_ignore
desc,          csg2_from_ignore
metadata,      csg2_from_ignore
style,         csg2_from_ignore
defs,          csg2_from_ignore
use,           csg2_from_ignore
symbol,        csg2_from_ignore
switch,        csg2_from_ignore
marker,        csg2_from_ignore
image,         csg2_from_ignore
foreignObject, csg2_from_ignore
