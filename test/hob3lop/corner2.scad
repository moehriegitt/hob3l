linear_extrude(height=1) {
    projection(){
        difference()
        {
            sphere(d = 50, $fn=30);
            for(i=[0:5]) {
                rotate([0,0,60*i]) {
                    difference() {
                        scale([1,3.3,1]) {
                            rotate([0,0,45]) {
                                translate([0,0,-50]) {
                                    cube([5, 5, 100]);
                                }
                            }
                        }
                        translate([0,0,-50]) {
                            cube([10,10,100]);
                        }
                    }
                }
            }
            translate([0,0,-40]) {
                sphere(d = 50, $fn=30);
            }
        }
    }
}
