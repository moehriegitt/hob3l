/* coincident tests */
translate([0,0,2]) {
    translate([0,0,-5]) {
        cube([20,20,10], center=true);
    }
    translate([0,0,+5]) {
        cylinder(h = 10, d1 = 15, d2=0, $fn = 5, center=true);
    }
}
