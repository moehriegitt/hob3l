translate([+15, +15, 0])
    sphere(10, $fn=7);

translate([-15, -15, 0])
    cube(15, center=true);

translate([+15, -15, 0])
    cylinder(h = 15, d1 = 12, d2 = 18, $fn = 5, center = true);

translate([-15, +15, 0])
    cylinder(h = 15, d1 = 18, d2 = 0,  $fn = 9, center = true);

translate([  0,   0, 0])
    linear_extrude(height = 15, slices = 4, center=true, twist = 80)
        translate([2, 0, 0])
            circle(6, $fn = 7);
