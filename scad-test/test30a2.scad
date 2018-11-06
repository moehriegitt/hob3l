/* coincident tests */
translate([0,0,1.9]) {
    // polygon(points=[[1,2],[3,4],[5,6]]);
    translate([0,0,-5]) {
        cube([20,20,10], center=true);
    }
    translate([0,0,+5]) {
        cylinder(h = 10, d1 = 15, d2=2, $fn = 5, center=true);
    }
}
