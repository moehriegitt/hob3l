linear_extrude(height=20, scale=[1,.8], twist=-30, slices=8) {
    translate([2,2,0]) {
        polygon(
            points=[
               [0,0],
               [10,0],
               [10,15],
               [0,15]],
            paths=[
               [0, 1, 2, 3],
            ]
        );
    }
}
