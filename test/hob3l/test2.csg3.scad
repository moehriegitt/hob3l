union(){
  rotate(a=[20,30,40]){
    difference(){
      cylinder(h=20,r1=40,r2=30,center=true,$fa=12,$fs=2,$fn=100);
      cylinder(h=200,r1=10,r2=10,center=true,$fa=12,$fs=2,$fn=100);
    }
  }
}
