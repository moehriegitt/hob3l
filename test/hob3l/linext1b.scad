rotate([10, -40, 0])
linear_extrude(height=20, scale=0, twist=30, slices=8) {
    translate([2,2,0]) square([10, 15]);
    rotate([0,0,150])
       translate([2,2,0]) square([10, 15]);
}
