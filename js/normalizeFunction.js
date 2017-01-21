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
