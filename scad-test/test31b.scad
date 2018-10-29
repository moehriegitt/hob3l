color("mediumturquoise") {
rotate([30,20,0])
  translate([0,10,0])
    scale([2,1,0.5])
      color("red")
      sphere(10, $fn=7);
cube(10);
translate([10,0,0])
   color("yellow")
     cube([1,1,20]);
}
