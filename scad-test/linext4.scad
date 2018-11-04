linear_extrude(height=20, twist=90, scale=[-1,-1], slices=3) {
    translate([6,6,0]) {
        circle(5, $fn=19);
    }
}
