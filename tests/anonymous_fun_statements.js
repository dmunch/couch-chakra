// -L

chai.should();
var ctx = evalcx('');

for(var i = 0; i < 1000; i++) {
  var fun = evalcx("function() { return " + i + ";};", ctx);
  var fun1 = evalcx("() => { return " + (i+1) + ";}", ctx);

  fun().should.equal(i);
  fun1().should.equal(i + 1);
}
