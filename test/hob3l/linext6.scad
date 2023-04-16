linear_extrude(height=20, slices=7, twist=20) {
    difference() {
        square(20, center=true);
        circle(8, $fn=20);
    }
}
