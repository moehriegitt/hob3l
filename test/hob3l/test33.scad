scale(20)
linear_extrude(height = 1) {
    polygon(
        points=[
            [-1,-1],
            [+1,+1],
            [+1,-1],
            [-1,+1]
        ]
    );
}