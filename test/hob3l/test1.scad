rotate(a = 10) multmatrix([[1,0,0,0],[0,1,0,0],[0.0,0,PI,0]]) {
    group() {
        cube(20, center = true);
        sphere(12, $fn = 32);
        cylinder(40, PI, center = true, $fn = 16);
    }
}
