translate([0,60,0])
   rotate_extrude(angle=270, convexity=10)
       translate([40, 0]) circle(10);
rotate_extrude(angle=90, convexity=10)
   translate([20, 0]) circle(10);
translate([20,0,0])
   rotate([90,0,0]) cylinder(r=10,h=80);
