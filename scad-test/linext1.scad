rotate([30, 40, 0])
linear_extrude(height=20, scale=[2,.5], twist=-30, slices=8) {
    translate([2,2,0]) square([10, 15]);
}
