// use <hob3l.scad>

cube(10);
rotate([0,0,30]) {
    cube(20);
    clude(in="foo") {
        cylinder(h=40, d=10);
    }
}
