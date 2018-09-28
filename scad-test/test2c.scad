union() {
    rotate([20,30,40]) {
        difference() {
            cylinder(20,40,0,true,$fn=100);
            // cylinder(200,10,10,true,$fn=100);
        }
    }
}
