/* use with -gran=1 */
linear_extrude(height = 10) {
    polygon(
        points=[
            [ 0, 0],
            [ 0, -1],
            [ 4, -5],
            [-4, -5],
            [-4, +5],
        ]
    );
    polygon(
        points=[
            [-1, +5],
            [+1, -2.5],
            [+5, +5],
        ]
    );
}
