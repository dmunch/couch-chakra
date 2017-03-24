let escodegen = require("escodegen");
let esprima = require("esprima-couch-chakra");
let Ernie = require("ernie.js");

let jasmineRequire = require("jasmine-core/lib/jasmine-core/jasmine.js");
let jasmineRequire_console = require("jasmine-core/lib/console/console.js");

function normalizeFunction(fun, fun1) {
	var ast = esprima.parse(fun);

  var funDeclaration = {};
  var idx = ast.body.length - 1;

  //search for the first FunctionDeclaration beginning from the end 
  do {
    funDeclaration = ast.body[idx--];
  } while(idx >= 0 && funDeclaration.type != "FunctionDeclaration")
  idx++;

  //if we have a function declaration with an Id, wrap it 
  //in an ExpressionStatement and change it into
  //a FuntionExpression
	if(funDeclaration.type == "FunctionDeclaration"
			&& funDeclaration.id == null){
    funDeclaration.type = "FunctionExpression";
    ast.body[idx] = {
       "type": "ExpressionStatement",
       "expression": funDeclaration 
    };
	}

  //re-generate the rewritten AST
	return escodegen.generate(ast);
}

function initJasmine(context) {
  context.setTimeout = function(fn, delay, params) { fn.apply(this, params); };
  context.setInterval = function(fn, delay, params) { print('si');print(fn.toString());};
  context.clearInterval = function(fn, delay, params) { print('ci');print(fn.toString());};
  context.clearTimeout = function(fn, delay, params) { };

  var jasmine = jasmineRequire.core(jasmineRequire);
  var env = jasmine.getEnv();
  var jI = jasmineRequire.interface(jasmine, env);
  extend(context, jI);

  var CoRep = jasmineRequire_console.ConsoleReporter();
  env.addReporter(new CoRep({print: context.console.log, showColors: true}));

  env.specFilter = function(spec) {
    //print(spec.getFullName());
    //for (var property in spec) print(property); 
    return true;
  };

  function extend(destination, source) {
    for (var property in source) destination[property] = source[property];
    return destination;
  }
  return env;
}

function runJasmine(env) {
  env.randomizeTests(1);
  env.execute();
}

module.exports = {
  normalizeFunction,
  Ernie,
  initJasmine,
  runJasmine
}
