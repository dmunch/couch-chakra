var a = "global_a";
var b = "global_b";
  
var sandbox1 = evalcx('');
var sandbox2 = evalcx('');
  
var fun1 = evalcx('() => b;', sandbox1);
var fun2 = evalcx('() => c;', sandbox2);

chai.should();

for(var i = 0; i < 1000; i ++) {
  sandbox1.b = "sandbox1b"; 
  sandbox2.c = "sandbox2c";

  fun1().should.equal('sandbox1b');
  fun2().should.equal('sandbox2c');

  sandbox1.b = "sandbox1b_1"; 
  sandbox2.c = "sandbox2c_1";

  fun1().should.equal('sandbox1b_1');
  fun2().should.equal('sandbox2c_1');

  a.should.equal('global_a');
  b.should.equal('global_b');
}
