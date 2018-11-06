rotate(a=30,v=[-1,1,0]) {
    linear_extrude(height=20) {
        difference() {
            square(20, center=true);
            translate([-5,-5]) {
                square(30);
            }
        }
    }
}
