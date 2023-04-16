rotate([0,-90,0]){
    difference(){
        cube([30, 30, 30]);
        translate([0,30,0]) {
            cube([30, 30, 30]);
        }
    }
}