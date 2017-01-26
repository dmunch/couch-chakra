// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ChakraCore.h>
#include <bert.h>

#include "couch_args.h"
#include "couch_readline.h"
#include "couch_readfile.h"

#include "../obj/main.js.h"

void beforeCollectFunWithContextCallback(JsRef funInContext, void* callbackState);

void create_function(JsValueRef object, char* name, JsNativeFunction fun, void* callbackState);
JsValueRef normalizeFunction(JsValueRef context, JsValueRef jsNormalizeFunction, JsValueRef funScript);
void printException(JsErrorCode error);
void printProperties(JsValueRef object);

#define JS_FUN_DEF(name) JsValueRef name( \
   JsValueRef callee,                     \
   bool isConstructCall,		              \
   JsValueRef *argv, 		                  \
   unsigned short argc,	                  \
   void *callbackState)

JS_FUN_DEF(readline);
JS_FUN_DEF(print);
JS_FUN_DEF(seal);
JS_FUN_DEF(gc);
JS_FUN_DEF(quit);
JS_FUN_DEF(evalcx);
JS_FUN_DEF(runInContext);

JS_FUN_DEF(readline)
{
  JsValueRef line = couch_readline(stdin);
  if(!line) {
    JsValueRef falseValue;
    JsGetFalseValue(&falseValue);
    return falseValue;
  }
    
  return line;
}

JS_FUN_DEF(print)
{
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  
  for(int a = 0; a < argc; a++){
    JsValueRef value = argv[a];
    size_t written;
    int stringLength;
   

    if(value == JS_INVALID_REFERENCE) {
      return trueValue;
    }

    JsValueType type;
    JsGetValueType(value, &type);
    
    if(type == JsUndefined) {
      continue;
    }

    JsGetStringLength(value, &stringLength);
    if(stringLength < 1) {
      continue;
    } 

    char *str = malloc(stringLength + 1);
    JsCopyString(value, str, stringLength, &written);
    str[stringLength] = 0;
    fprintf(stdout, "%s", str);
    free(str);
  }
  fputc('\n', stdout);
  fflush(stdout);

  return trueValue;
}

JS_FUN_DEF(seal)
{
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  return trueValue;
}

JS_FUN_DEF(gc)
{
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  
  JsRuntimeHandle runtime = (JsRuntimeHandle) callbackState;
  JsCollectGarbage(runtime);

  return trueValue;
}

JS_FUN_DEF(quit)
{
  JsValueRef falseValue;
  JsGetTrueValue(&falseValue);
  
  if(argc != 2) {
    return falseValue;
  }
  int exitCode;
  if(JsNumberToInt(argv[1], &exitCode) != JsNoError) {
    return falseValue;
  }
  exit(exitCode);
}

typedef struct {
  JsValueRef fun;
  JsContextRef context;
} FunWithContext;

//wrapper function to call a given function in another context.
JS_FUN_DEF(runInContext)
{
  FunWithContext *funWithContext = (FunWithContext*) callbackState;
  JsValueRef result;
  JsContextRef oldContext;

  JsGetCurrentContext(&oldContext);
  JsSetCurrentContext(funWithContext->context);
  JsCallFunction(funWithContext->fun, argv, argc, &result);
  JsSetCurrentContext(oldContext);

  return result;
}

JsValueRef normalizeFunction(JsValueRef context, JsValueRef jsNormalizeFunction, JsValueRef funScript)
{
  JsValueRef normalized;
  JsValueRef undefined;
  JsGetUndefinedValue(&undefined);

  JsValueRef argv[] = {undefined, funScript};
  JsCallFunction(jsNormalizeFunction, argv, 2, &normalized);
  return normalized;
}

void beforeCollectFunWithContextCallback(JsRef funInContext, void* callbackState)
{
  FunWithContext *funWithContext = (FunWithContext*) callbackState;

  //The function can now be freed for garbage collection.
  JsRelease(funWithContext->fun, NULL);
  free(funWithContext);
}

typedef struct {
  couch_args* args;
  JsRuntimeHandle runtime;
  JsValueRef normalizeFunction;
} EvalCxContext; 

JS_FUN_DEF(evalcx)
{
  if(argc < 2) {
    JsValueRef falseValue;
    JsGetTrueValue(&falseValue);
    return falseValue;
  }
  JsValueRef sandbox = JS_INVALID_REFERENCE;
  JsValueRef script = JS_INVALID_REFERENCE;
  JsValueRef name = JS_INVALID_REFERENCE;
  if(argc > 1) {
    script = argv[1];
  }
  if(argc > 2) {
    sandbox = argv[2];
  }
  if(argc > 3) {
    name = argv[3];
  } else {
  }

  if(name == JS_INVALID_REFERENCE) {
    JsCreateString("no-name", strlen("no-name"), &name);
  } else {
    JsValueType nameType;
    JsGetValueType(name, &nameType);
    if(nameType != JsString) {
      JsCreateString("no-name", strlen("no-name"), &name);
    }
  }

  EvalCxContext* evalCxContext = (EvalCxContext*) callbackState;
  JsContextRef context;
  JsContextRef oldContext;
  JsGetCurrentContext(&oldContext);
  
  if(sandbox == JS_INVALID_REFERENCE) {
     //we need to create a new sandbox
     JsCreateContext(evalCxContext->runtime, &context);

     JsSetCurrentContext(context);
     JsGetGlobalObject(&sandbox);
     //curently only for debugging purposes  
     create_function(sandbox, "print", print, NULL);
     JsSetCurrentContext(oldContext);
  } else {
    JsGetContextOfObject(sandbox, &context);
  }
  
  if(script == JS_INVALID_REFERENCE) {
    //no script given, only create sandbox and return
    return sandbox;
  }
  
  int scriptLength;
  JsGetStringLength(script, &scriptLength);

  if(scriptLength < 1) {
    //no script given, only create sandbox and return
    return sandbox;
  }

  if(evalCxContext->args->use_legacy) {
    script = normalizeFunction(context, evalCxContext->normalizeFunction, script);  
  }
  
  JsSetCurrentContext(context);
  JsValueRef fun;
  JsRun(script, JS_SOURCE_CONTEXT_NONE, name, JsParseScriptAttributeNone, &fun);
  
  //We need to increase the reference count if the function
  //otherwise it gets garbage collected at some point.
  //The corresponding JsRelease call is done in beforeCollectFunWithContextCallback()
  JsAddRef(fun, NULL);

  //Intuitevely I would have used JsParse here, however it doesn't seem to work
  //as expected. JsRun works fine though, so we use that at the moment.
  //JsParse(str22, JS_SOURCE_CONTEXT_NONE, name, JsParseScriptAttributeNone, &fun);
  
  
  JsSetCurrentContext(oldContext);
 
  //The corresponding free is done in beforeCollectFunWithContextCallback()
  FunWithContext *funWithContext = (FunWithContext*) malloc(sizeof(FunWithContext));
  funWithContext->fun = fun;
  funWithContext->context = context; 

  JsValueRef funInContext;
  JsCreateFunction(runInContext, funWithContext, &funInContext);

  //we need this to tidy up, e.g. call JsRelease and free.
  JsSetObjectBeforeCollectCallback(
      funInContext, 
      funWithContext,
      beforeCollectFunWithContextCallback);
  
  return funInContext;
}

void create_function(JsValueRef object, char* name, JsNativeFunction fun, void* callbackState)
{
  JsValueRef funHandle;
  JsCreateFunction(fun, callbackState, &funHandle);

  JsPropertyIdRef propId;
  JsCreatePropertyId(name, strlen(name), &propId);

  JsSetProperty(object, propId, funHandle, false);
}

void printProperties(JsValueRef object)
{
  JsValueRef propertyNames = JS_INVALID_REFERENCE;
  JsErrorCode error;

  JsGetOwnPropertyNames(object, &propertyNames);

  unsigned int i = 0;
  bool hasValue;
  JsValueRef index;

  do 
  {
    JsIntToNumber(i++, &index);

    error = JsHasIndexedProperty(propertyNames, index, &hasValue);
    if(!hasValue) {
      continue;
    }

    JsValueRef propName;
    JsGetIndexedProperty(propertyNames, index, &propName);

    //JsPropertyIdRef propId;
    //JsValueRef propValue;
    //JsCreatePropertyId((char*) binary.data, binary.size, &propId);
    //JsGetProperty(value, propId, &propValue);
    print(NULL, false, &propName, 1, NULL);
    } while(error == JsNoError && hasValue);
}

void printException(JsErrorCode error)
{
  fprintf(stderr, "\n\n=== ERROR === \nretcode: 0x%X\n", error); 

  bool hasException;
  JsHasException(&hasException);

  if(!hasException) {
    return;
  }

  JsValueRef exception;
  JsGetAndClearException(&exception);
    
  JsValueRef lineNumber;
  JsValueRef columnNumber;
  JsPropertyIdRef property;

  JsCreatePropertyId("line", strlen("line"), &property);
  JsGetProperty(exception, property, &lineNumber);
    
  JsCreatePropertyId("column", strlen("column"), &property);
  JsGetProperty(exception, property, &columnNumber);
    
  int line;
  int column;
  JsNumberToInt(lineNumber, &line);
  JsNumberToInt(columnNumber, &column);
    
  JsValueRef strException;
  JsConvertValueToString(exception, &strException);
	 
  fprintf(stderr, "has exception at \n");
  fprintf(stderr, "line %d\n", line);
  fprintf(stderr, "column %d\n", column);
  print(NULL, false, &strException, 1, NULL);
  fprintf(stderr, "list of properties on error object:\n");
  printProperties(exception); 
}

JsValueRef bert_to_js(bert_data_t* data) 
{
  JsValueRef value;
  switch(data->type) {
    case bert_data_none:
      break;
    case bert_data_boolean:
      break;
    case bert_data_int:
         JsIntToNumber(data->integer, &value);
      break;
    case bert_data_float:
      break;
    case bert_data_atom:
      break;
    case bert_data_string:
      JsCreateString(data->string.text, data->string.length, &value);
      break;
    case bert_data_bin:
      break;
    case bert_data_tuple:
      break;
    case bert_data_list: {
      int length = bert_list_length(data->list);
      JsCreateArray(length, &value);
      int i = 0;
      JsValueRef index;
      for(bert_list_node_t* node = data->list->head; node; i++, node = node->next) {
         JsIntToNumber(i, &index);
         JsSetIndexedProperty(value, index, bert_to_js(node->data));
      }
      break;
     }
    case bert_data_dict:
      break;
    case bert_data_time:
      break;
    case bert_data_regex:
      break;
    case bert_data_nil:
        break;
  }
  return value;
}
bert_decoder_t *decoder;

JS_FUN_DEF(bertReadTerm)
{
  JsValueRef falseValue;
  JsGetFalseValue(&falseValue);
  
//  bert_decoder_t *decoder = bert_decoder_create();
//  bert_decoder_stream(decoder, STDIN_FILENO);

  bert_data_t *data;
  int result;
  
  // decode BERT data
  if ((result = bert_decoder_pull(decoder, &data)) != 1)
  {
    fprintf(stderr,"bert error: %s\n", bert_strerror(result));

    //bert_decoder_destroy(decoder);
    return falseValue;
  }

  fprintf(stderr,"BERT data is %x\n", data->type);
  /*
  if (data->type != bert_data_tuple)
  {
    fprintf(stderr,"BERT data was not a tuple but %x\n", data->type);

    bert_data_destroy(data);
    bert_decoder_destroy(decoder);
    
    return falseValue;
  }

  fprintf(stderr, "BERT tuple decoded with %d elements\n",data->tuple->length);

  //JsValueRef obj;
  //JsCreateObject(context, &obj);
  for(int i = 0; i < data->tuple->length; i++) {
    
    if(data->tuple->elements[i]->type != bert_data_string) {
      fprintf(stderr,"BERT data was not a string but %x\n", data->tuple->elements[i]->type);
    }
  }
  */

  JsValueRef value = bert_to_js(data);
  bert_data_destroy(data);
  //bert_decoder_destroy(decoder);
  return value;
}

int main(int argc, const char* argv[])
{
  decoder = bert_decoder_create();
  bert_decoder_stream(decoder, STDIN_FILENO);
  
    JsRuntimeHandle runtime;
    JsContextRef context;
    JsErrorCode error;

    couch_args* args = couch_parse_args(argc, argv);
    
    JsCreateRuntime(JsRuntimeAttributeNone, NULL, &runtime);

    if(args->stack_size > 0) {
      JsSetRuntimeMemoryLimit(runtime, args->stack_size);  
    }

    JsCreateContext(runtime, &context);

    JsSetCurrentContext(context);
    JsValueRef globalObject;
    JsGetGlobalObject(&globalObject);

    
    EvalCxContext *evalCxContext = (EvalCxContext*) malloc(sizeof(EvalCxContext));
    evalCxContext->args = args;
    evalCxContext->runtime = runtime;

    create_function(globalObject, "readline", readline, NULL);
    create_function(globalObject, "bertReadTerm", bertReadTerm, NULL);
    create_function(globalObject, "print", print, NULL);
    create_function(globalObject, "seal", seal, NULL);
    create_function(globalObject, "gc", gc, runtime);
    create_function(globalObject, "exit", quit, NULL);
    create_function(globalObject, "evalcx", evalcx, evalCxContext);

    if(evalCxContext->args->use_legacy) {
      JsValueRef mainSrc;
      JsValueRef mainHref;
      JsValueRef mainRes;

      JsCreateString((const char*) obj_main_js, obj_main_js_len, &mainSrc);
      JsCreateString("main.js", strlen("main.js"), &mainHref);
      error = JsRun(mainSrc, JS_SOURCE_CONTEXT_NONE, mainHref, JsParseScriptAttributeNone, &mainRes);
      if(error != JsNoError) {
        printException(error);
      }
     
      JsValueRef funId; 
      JsCreatePropertyId("normalizeFunction", strlen("normalizeFunction"), &funId);
      JsGetProperty(globalObject, funId, &evalCxContext->normalizeFunction);
    }

    for(int i = 0 ; args->scripts[i] ; i++) {
      JsValueRef script = couch_readfile(args->scripts[i]);
      if(!script) {
          return 1;
      }
     
      JsValueRef result;
      JsValueRef sourceHref;
      JsCreateString(args->scripts[i], strlen(args->scripts[i]), &sourceHref);
      error = JsRun(script, JS_SOURCE_CONTEXT_NONE, sourceHref, JsParseScriptAttributeNone, &result);
       
      if(error != JsNoError && args->debug) {
        printException(error);
      } 
    }
   
    free(evalCxContext); 
    bert_decoder_destroy(decoder);
    JsSetCurrentContext(JS_INVALID_REFERENCE);
    JsDisposeRuntime(runtime);

    if(error != JsNoError) {
      return 1;
    }
    return 0;
}
