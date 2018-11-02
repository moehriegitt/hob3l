scale(5) {
  translate([0,8,0]) {
    linear_extrude(height = 10, center = true, scale=[1,5], twist=90) {
      difference() {
        circle(r = 1);
        square([0.5, 2]);
      }
    }
  }
  translate([-2,0,0]) {
    linear_extrude(height = 10, center = true, scale=[1,5], twist=90) {
      circle(r = 1);
      color("red")
        square([0.5, 2]);
    }
  }
  translate([+2,0,0]) {
    linear_extrude(height = 10, center = true, scale=[1,5], twist=90) {
      circle(r = 1);
    }
    linear_extrude(height = 10, center = true, scale=[1,5], twist=90) {
      square([0.5, 2]);
    }
  }
}

