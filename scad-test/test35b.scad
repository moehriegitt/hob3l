rotate([20,0,0])
rotate_extrude($fn = 10, angle=270) {
   rotate([0,0,30]) translate([15,0,0]) scale([1.5,0.5]) circle(5, $fn=6);
   translate([20,0,0]) scale([2,1]) circle(5, $fn = 6);
   rotate([0,0,-30]) translate([7.5,0,0]) scale([1.5,0.5]) circle(5, $fn=6);
}
