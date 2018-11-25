scale(20)
linear_extrude(height = 1) {
    polygon(
        points=[
            [-1,-1],
            [+1,+1],
            [+2,-0.5],
            [-1,+1]
        ]
    );
    // translate([3,3]) square(0.1);
}