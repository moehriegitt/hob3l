difference() {
    rotate([0,0,+30]) {
        cube(10, center=true);
        translate([-10,-10,0])
            cube(10, center=true);
    }
}
