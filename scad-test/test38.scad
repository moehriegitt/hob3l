linear_extrude(height=5)
hull() 
union() {
    difference() {
        square(10);
        translate([-.1, -.1]) square(9);
    }
    difference() {
        square(10, center=true);
        square(8, center=true);
    }
}
