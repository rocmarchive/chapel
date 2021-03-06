// Checks the behavior for declaring an argument with the type desired
class Foo {
  type t;
  var x;

  proc init(xVal) {
    t = xVal.type;
    x = xVal;
    super.init();
  }
}

proc takesAFoo(val: Foo(int, int)) {
  writeln(val);
  writeln(val.type: string);
}

var f = new Foo(10);
takesAFoo(f);
delete f;
