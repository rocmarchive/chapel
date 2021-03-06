// This test exercises inheriting from a generic class when the parent has a
// generic var field and the child is concrete.
class Parent {
  var gen;
  var x: int;

  proc init(xVal: int) {
    gen = -xVal;
    x = xVal;
    super.init();
  }
}

class Child : Parent {
  var gen2;
  var y: int;

  proc init(yVal: int, xVal: int) {
    gen2 = -yVal;
    y = yVal;
    super.init(xVal);
  }
}

proc main() {
  var child = new Child(10, 11);
  writeln(child.type:string);
  writeln(child);
  delete child;
}
