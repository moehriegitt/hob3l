scale(10)
linear_extrude(height=1) {
    rotate([0,0,180])
    polygon(
        points=[
            [-2,+2],
            [+1, 0],
            [-1.5,+0.5],
            [-1.5,-0.5],
            [-2,-2],
        ],
        paths=[
            [0,1,2,3,1,4]
        ],
    );
}
