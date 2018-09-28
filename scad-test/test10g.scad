rotate([0,0,+30]) {
    difference() {
        cube(10, center=true);
        translate([-8,-8,0])
            cube(10, center=true);
    }
}
