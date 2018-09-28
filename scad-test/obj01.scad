difference() {
    rotate([10,15,20]) {
        rotate(a = 5, v=[0,0,1000]) {
            cylinder(h = 20, d1 = 10, d2 = 20, $fn = 5);
        }
        scale([1.1, 0.9, 0.8]) {
            rotate(a = 10, v=[1000,1000,0]) {
                translate([15,0,-5]) {
                    cylinder(h = 30, d1 = 10, d2 = 0, $fn = 7, center=true);
                }
            }
        }
        rotate(a = 20, v=[0,1000,0]) {
            cube([30,20,10], center=true);
        }
    }
    rotate(a = 20, v=[0,1000,0]) {
        translate([0,0,-100]) {
            cube([200,200,200], center=true);
        }
    }
}
