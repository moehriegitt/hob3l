rotate([10,0,0]) {
    translate([0,0,-5]) rotate_extrude($fn=5) translate([-11,8]) circle(10, $fn=3);
    translate([0,0,+5]) rotate_extrude($fn=5) translate([  8,8]) circle(10, $fn=3);
}
