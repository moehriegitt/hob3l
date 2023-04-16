import("nozzle2.stl");
translate([3,3,20]) {
    cube([0.1,0.1,100], center=true);
    cube([0.1,100,0.1], center=true);
    cube([100,0.1,0.1], center=true);
}
