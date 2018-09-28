difference() {
    translate([0, 0, 0]) {
        cylinder(h = 30, d = 10, $fn = 10);
    }
    difference() {
        translate([5,0,10]) {
            cube([10, 5, 6], center=true);
        }
        translate([0, 0, 10])
            rotate([0, 45, 0])
                translate([-10, 0, 0])
                    cube([20, 20, 20], center=true);
    }
}