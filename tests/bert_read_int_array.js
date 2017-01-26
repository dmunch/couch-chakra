// 
// ./tests/bert.erl "[266, 267]" "[\"a\", \"b\"]"
// cat test_int_list_2.bert test.bert
// test.bert

var a = readline_bert();

chai.should();
a.should.deep.equal([266, 267]);

var b = readline_bert();
b.should.deep.equal(["a", "b"]);
