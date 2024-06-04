%{
typedef struct {
    char const id[{maxlen}+1];
    cp_scad_type_t type;
    bool (*from)(
        ctxt_t *t,
        cp_syn_stmt_item_t *f,
        cp_scad_t *r);
} {value_t};
%}
prefix: cmd_
key: id
%%
circle,          CP_SCAD_CIRCLE,       circle_from_item
color,           CP_SCAD_COLOR,        color_from_item
cube,            CP_SCAD_CUBE,         cube_from_item
cylinder,        CP_SCAD_CYLINDER,     cylinder_from_item
difference,      CP_SCAD_DIFFERENCE,   difference_from_item
group,           CP_SCAD_UNION,        union_from_item
hull,            CP_SCAD_HULL,         hull_from_item
import,          CP_SCAD_IMPORT,       import_from_item
import_stl,      CP_SCAD_IMPORT,       import_from_item
intersection,    CP_SCAD_INTERSECTION, intersection_from_item
linear_extrude,  CP_SCAD_LINEXT,       linext_from_item,
mirror,          CP_SCAD_MIRROR,       mirror_from_item
multmatrix,      CP_SCAD_MULTMATRIX,   multmatrix_from_item
polygon,         CP_SCAD_POLYGON,      polygon_from_item
polyhedron,      CP_SCAD_POLYHEDRON,   polyhedron_from_item
projection,      CP_SCAD_PROJECTION,   projection_from_item
render,          CP_SCAD_UNION,        union_from_item
rotate,          CP_SCAD_ROTATE,       rotate_from_item
rotate_extrude,  CP_SCAD_ROTEXT,       rotext_from_item,
scale,           CP_SCAD_SCALE,        scale_from_item
sphere,          CP_SCAD_SPHERE,       sphere_from_item
square,          CP_SCAD_SQUARE,       square_from_item
surface,         CP_SCAD_SURFACE,      surface_from_item
text,            CP_SCAD_TEXT,         text_from_item,
translate,       CP_SCAD_TRANSLATE,    translate_from_item
union,           CP_SCAD_UNION,        union_from_item
{,               CP_SCAD_UNION,        union_from_item
