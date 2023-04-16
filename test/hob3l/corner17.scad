scale([1,1,0.6]) {
    translate([0,0,-5]) {
        difference() {
            union() {
                cube(10);
                cylinder(10, r=10, $fn = 20);
            }
            translate([-20,0,-5]) {
                cube(20);
            }
        }
    }
}
