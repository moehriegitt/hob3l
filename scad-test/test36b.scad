difference() {
    linear_extrude(height=10) {
        projection(cut = true) translate([0,0,+0.1]) cube(8);
    }
    sphere(5);
}
