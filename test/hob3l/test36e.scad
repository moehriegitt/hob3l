translate([0,0,-10]) {
    rotate_extrude($fn=200) {
        polygon(
            points=[
                [0, 0],
                [0, 10],
                [20, 20],
                [15, 5],
            ]
        );
    }
    translate([15,10,20]) scale([10,20,30]) scale(0.5) sphere($fn=200);
}