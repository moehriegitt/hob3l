rotate([0,0,88])
rotate([0,-90,0]){
    difference(){
        rotate([-45,0,0]) {
            cube([30, 30, 30]);
        }
        scale([1,2,1]) {
            rotate([-45,0,0]) {
                cube([30, 30, 30]);
            }
        }
    }
}