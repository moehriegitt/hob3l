translate([-30,50,0])
scale([3,3,1])
rotate([0,0,-2]) {
    difference() {
        cube([20, 20, 20]);
        translate([7, 0, -1]) {
            rotate([0,0,50]) {
                cube([30, 30, 30]);
            }
        }
        translate([13, 0, -1]) {
            rotate([0,0,50]) {
                cube([30, 30, 30]);
            }
        }
    }
}
