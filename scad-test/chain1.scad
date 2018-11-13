linear_extrude(height=4) {
    polygon(
        points=[
            [  0, 0 ],
            [  5, 0 ],
            [ 10,-5 ],
        ]
    );
}
linear_extrude(height=4) {
    polygon(
        points=[
            [ -5,  0 ],
            [  0,  0 ],
            [ 10, +5 ],
        ],
        paths = undef
    );
}
