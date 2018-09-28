difference() {
    group() {
        /* %cylinder(h = 20, d = 10, center=true, $fn = 32); */
    }
    rotate([0,90,0]) {
        {
        cylinder(h = 40, d = 7, center=true, $fn = 32);
        }
    }
    rotate([90,0,0]) {
        %cylinder(h = 50, d = 5, center=true, $fn = 32);
    }
}
