/* use with -gran=0.5 */
linear_extrude(height = 10) {
    polygon(
        points=[
            [ 0, 0],
            [ 0, -1],
            [ 1, -5],
            [-4, -5],
            [-4, +5],
        ]
    );
    polygon(
        points=[
            [-1, +5],
            [+1, -3],
            [+5, +5]
        ]
    );
}
