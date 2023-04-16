/* coincident tests */
translate([0,0,2]) {
    translate([0,0,-5]) {
        cube([20,20,10], center=true);
    }
    translate([-7,0,0]) {
        rotate([45,0,0]) {
            cube([15,15,15]);
        }
    }
}
