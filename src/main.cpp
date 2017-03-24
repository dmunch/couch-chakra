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
#include <queue>

#include <ChakraCore.h>
#include <uv.h>

#include "couch_args.h"
#include "couch_readline.h"
#include "couch_readfile.h"

#include "../dist/couch_chakra.js.h"

/* Makes the given file descriptor non-blocking.
 *  * Returns 1 on success, 0 on failure.
 *  */
int make_blocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1) /* Failed? */
		return 0;
	/* Clear the blocking flag. */
	flags &= ~O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags) != -1;
}

void beforeCollectFunWithContextCallback(JsRef funInContext, void* callbackState);

void create_function(JsValueRef object, char* name, JsNativeFunction fun, void* callbackState);
JsValueRef normalizeFunction(JsValueRef context, JsValueRef jsNormalizeFunction, JsValueRef funScript);
void printException(JsErrorCode error);
void printProperties(JsValueRef object);

JsValueRef couch_readfile(const char* filename)
{
    JsValueRef string;
    size_t byteslen;
    char *bytes;

    if((byteslen = slurp_file(filename, &bytes))) {
        JsCreateString(bytes, byteslen, &string);
        return string;
    }
    return NULL;    
}

JsValueRef couch_readline(FILE* fp)
{
    size_t byteslen;
    char* bytes = couch_readline_(fp, &byteslen);
    JsValueRef str;
    JsCreateString(bytes, byteslen, &str);
    free(bytes);
    return str;
}

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

struct PrintOptions {
  inline PrintOptions(FILE* f, bool nl) : file(f), newLine(nl) {}
  
  FILE* file;
  bool newLine;
};

JS_FUN_DEF(print)
{
  auto options = (PrintOptions*) callbackState;
  
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  for(int a = 0; a < argc; a++){
    JsValueRef value = argv[a];

    if(value == JS_INVALID_REFERENCE) {
      return trueValue;
    }

    JsValueType type;
    JsGetValueType(value, &type);
    
    if(type == JsUndefined) {
      continue;
    }
    
    if(type == JsTypedArray) {
      unsigned char* buffer;
      unsigned int bufferLength;
      JsTypedArrayType typedArrayType;
      int elementSize;

      JsGetTypedArrayStorage(value, &buffer, &bufferLength, &typedArrayType, &elementSize);
      fwrite(buffer, elementSize, bufferLength, options->file); 
      fflush(options->file);
      return trueValue;
    }

    size_t written;
    size_t bufferSize;
    JsCopyString(value, NULL, 0, &bufferSize);
    if(bufferSize < 1) {
      continue;
    } 

    char *str = (char *) malloc(bufferSize + 1);
    JsCopyString(value, str, bufferSize, &written);
    str[bufferSize] = 0;
    fprintf(options->file, "%s", str);
    
    free(str);
  }

  if(options->newLine) {
    fputc('\n', options->file);
    fflush(options->file);
  }

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
     create_function(sandbox, "print", print, new PrintOptions(stdout, false));
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


JS_FUN_DEF(TextEncoder_encode)
{
  size_t buffSize;
  size_t written;
  JsValueRef arrayBuffer;
  BYTE *arrayBufferStorage;
  unsigned int arrayBufferSize;

  JsCopyString(argv[1], NULL, 0, &buffSize);

  JsCreateArrayBuffer(buffSize, &arrayBuffer);
  JsGetArrayBufferStorage(arrayBuffer, &arrayBufferStorage, &arrayBufferSize);
  JsCopyString(argv[1], (char *) arrayBufferStorage, arrayBufferSize, &written);

  JsValueRef typedArray;
  JsCreateTypedArray(JsArrayTypeUint8, arrayBuffer, 0, buffSize, &typedArray);
  return typedArray;
}

JS_FUN_DEF(TextDecoder_decode)
{
  JsValueRef arrayBuffer;
  BYTE *arrayBufferStorage;
  unsigned int arrayBufferSize;

  //JsGetArrayBufferStorage(argv[1], &arrayBufferStorage, &arrayBufferSize);
  JsGetDataViewStorage(argv[1], &arrayBufferStorage, &arrayBufferSize);
  
  if(arrayBufferSize == 0) {
    JsValueRef falseValue;
    JsGetFalseValue(&falseValue);
    return falseValue;
  }
  
  JsValueRef string;
  JsCreateString((const char *) arrayBufferStorage, arrayBufferSize, &string);
  return string;
}

JS_FUN_DEF(TextEncoderConstructor) 
{
  create_function(argv[0], "encode", TextEncoder_encode, NULL); 
  return argv[0];
}
JS_FUN_DEF(TextDecoderConstructor) 
{
  create_function(argv[0], "decode", TextDecoder_decode, NULL);
  return argv[0];
}

JS_FUN_DEF(write)
{
  JsValueType type;
  JsGetValueType(argv[1], &type);
    
  if(type != JsTypedArray) {
    JsValueRef falseValue;
    JsGetFalseValue(&falseValue);
    return falseValue;
  }
    
  unsigned char* buffer;
  unsigned int bufferLength;
  JsTypedArrayType typedArrayType;
  int elementSize;

  JsGetTypedArrayStorage(argv[1], &buffer, &bufferLength, &typedArrayType, &elementSize);
  fwrite(buffer, elementSize, bufferLength, stdout); 
  fflush(stdout);
  
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  return trueValue;
}

void externalArrayBufferFinalizer(void *data) {
/*	if(data) {
		free(data);
	}
	*/
}

char megaBuffer[1048576];

JS_FUN_DEF(read)
{
  int size;
  JsNumberToInt(argv[1], &size);
    
  if(size <= 0) {
    JsValueRef falseValue;
    JsGetFalseValue(&falseValue);
    return falseValue;
  }
 
//  char* buffer = (char*) malloc(size); 
  char* buffer = megaBuffer;
  size_t actuallyRead = fread(buffer, 1, size, stdin);

  if(actuallyRead == 0) {
    // We handle error here
	  if (ferror (stdin))
	  {
		  fprintf (stderr, "We encountered an error!%d", 1);
		  perror ("Error message");
	  }
  }
  JsValueRef arrayBuffer;
  JsCreateExternalArrayBuffer(buffer, actuallyRead, externalArrayBufferFinalizer, buffer, &arrayBuffer);
  return arrayBuffer;
}


JS_FUN_DEF(exit_uv) 
{
  uv_stop((uv_loop_t*) callbackState);
  return argv[0];
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


uv_loop_t* loop; 
uv_loop_t* uv_chakra_init(JsValueRef globalObject);
void uv_chakra_run(uv_loop_t* loop);
void promiseContinuationCallback(JsValueRef task, void *callbackState);


int main(int argc, const char* argv[])
{
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
    JsSetPromiseContinuationCallback(promiseContinuationCallback, NULL);

    
    EvalCxContext *evalCxContext = (EvalCxContext*) malloc(sizeof(EvalCxContext));
    evalCxContext->args = args;
    evalCxContext->runtime = runtime;

    create_function(globalObject, "readline", readline, NULL);
    create_function(globalObject, "print", print, new PrintOptions(stdout, false));
    create_function(globalObject, "print_e", print, new PrintOptions(stderr, true));
    create_function(globalObject, "seal", seal, NULL);
    create_function(globalObject, "gc", gc, runtime);
    create_function(globalObject, "exit", quit, NULL);
    create_function(globalObject, "evalcx", evalcx, evalCxContext);
    create_function(globalObject, "TextEncoder", TextEncoderConstructor, NULL);
    create_function(globalObject, "TextDecoder", TextDecoderConstructor, NULL);
    create_function(globalObject, "read", read, NULL);
    create_function(globalObject, "write", write, NULL);
    
    uv_loop_t* loop; 
    if(args->use_evented) {
      fprintf(stderr, "initialize event loop %d", 1);    
      loop = uv_chakra_init(globalObject); 
      create_function(globalObject, "exit_uv", exit_uv, loop);
    } else {
      fprintf(stderr, "set sdtio to blocking %d", 1);    
    	//make_blocking(0);
    }

    JsValueRef mainSrc;
    JsValueRef mainHref;
    JsValueRef mainRes;

    JsCreateString((const char*) dist_couch_chakra_js, dist_couch_chakra_js_len, &mainSrc);
    JsCreateString("couch_chakra.js", strlen("couch_chakra.js"), &mainHref);
    error = JsRun(mainSrc, JS_SOURCE_CONTEXT_NONE, mainHref, JsParseScriptAttributeNone, &mainRes);
    if(error != JsNoError) {
	    printException(error);
    }
     
    if(evalCxContext->args->use_legacy) {
      JsValueRef moduleId; 
      JsCreatePropertyId("couch_chakra", strlen("couch_chakra"), &moduleId);
      
      JsValueRef funId; 
      JsCreatePropertyId("normalizeFunction", strlen("normalizeFunction"), &funId);
      
      JsValueRef module;
      JsGetProperty(globalObject, moduleId, &module);
      JsGetProperty(module, funId, &evalCxContext->normalizeFunction);
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

    if(args->use_evented) {
      uv_chakra_run(loop);
    }

    free(evalCxContext); 
    JsSetCurrentContext(JS_INVALID_REFERENCE);
    JsDisposeRuntime(runtime);

    if(error != JsNoError) {
      return 1;
    }
    return 0;
}

//Ideally, these would be in their own module.
//However, there's currently a ChakraCore issue 
//with duplicate symbols on linking
//https://github.com/Microsoft/ChakraCore/issues/2574

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

//uv_pipe_t stdin_pipe;
uv_tty_t stdin_pipe;
uv_pipe_t stdout_pipe;
uv_async_t async;

JsValueRef create_promise(JsValueRef callback)
{
  JsValueRef promiseFunction; 
  JsValueRef promiseFunId; 
  JsValueRef promise;
  JsCreatePropertyId("Promise", strlen("Promise"), &promiseFunId);
    
  JsValueRef globalObject;
  JsGetGlobalObject(&globalObject);
  JsGetProperty(globalObject, promiseFunId, &promiseFunction);

  JsValueRef undefined = JS_INVALID_REFERENCE;
  JsGetUndefinedValue(&undefined); 
  
  JsValueRef args[] = { undefined, callback };
  JsConstructObject(promiseFunction, args, 2, &promise);
  JsAddRef(promise, NULL);
  return promise;
}

typedef struct {
  JsValueRef resolve;
  JsValueRef reject;
} read_req_t;

JS_FUN_DEF(read_async_callback) {
  auto read_req = new read_req_t();
  read_req->resolve= argv[1];
  read_req->reject = argv[2];

  JsAddRef(read_req->resolve, NULL);
  JsAddRef(read_req->reject, NULL);

  //uv_stream_t* in_stream = (uv_stream_t*) callbackState;
  
  uv_stream_t* in_stream = (uv_stream_t*) &stdin_pipe;
  in_stream->data = read_req;  
  uv_read_start(in_stream, alloc_buffer, read_stdin);
}


JS_FUN_DEF(read_async)
{
  JsValueRef funHandle;
  JsCreateFunction(read_async_callback, callbackState, &funHandle);
  return create_promise(funHandle);
} 

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
  JsValueRef resolve;
  JsValueRef reject;
} write_req_t;

void write_callback(uv_write_t *req, int status) {
  write_req_t *wr = (write_req_t*) req;
    
  JsValueRef undefined;
  JsValueRef result; 
  JsGetUndefinedValue(&undefined);

  JsValueRef argv[] = {undefined};
  JsCallFunction(wr->resolve, argv, 1, &result);
    
  free(wr->buf.base);
  free(wr);
}

JS_FUN_DEF(write_async_callback)
{
  JsValueRef resolve = argv[1];
  JsValueRef reject = argv[2];
  
  void** argv_ = (void**) callbackState; 
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
 
  JsValueRef value = ((JsValueRef*)argv_[1])[1]; 
  int stringLength;
  size_t written;
  
  if(value == JS_INVALID_REFERENCE) {
    return trueValue;
  }

  JsValueType type;
  JsGetValueType(value, &type);
    
  if(type == JsUndefined) {
    return trueValue;
  }

  JsGetStringLength(value, &stringLength);
  if(stringLength < 1) {
    return trueValue;
  } 

  char *str = (char*) malloc(stringLength + 1);
  JsCopyString(value, str, stringLength, &written);
  str[stringLength] = 0;
 
  uv_stream_t* out_stream = (uv_stream_t*) argv_[0]; 
  
  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.base = str;
  req->buf.len = stringLength; 
  req->resolve = resolve;
  req->reject = reject;
  uv_write((uv_write_t*) req, out_stream, &req->buf, 1, write_callback);
  
  return trueValue;
}

JS_FUN_DEF(write_async) {
  JsValueRef funHandle;
  void* argv_[] = {callbackState, argv};
  JsCreateFunction(write_async_callback, argv_, &funHandle);
  return create_promise(funHandle);
}

std::queue<JsValueRef> taskQueue; 
void promiseContinuationCallback(JsValueRef task, void *callbackState)
{
  JsContextRef context;

  // need to add a referenc to that task, otherwise
  // the garbage collector will collect it
  JsAddRef(task, NULL);
  taskQueue.push(task);
  uv_async_send(&async);
}

static void async_callback(uv_async_t* watcher) {
  JsValueRef result;
  JsValueRef global;
  
  // Execute promise tasks stored in taskQueue
  if(!taskQueue.empty()) {
    JsValueRef task = taskQueue.front();
    taskQueue.pop();
   
    JsGetGlobalObject(&global); 
    JsValueRef args[] = {global};
    JsCallFunction(task, args, 1, &result);

    // we can now savely release the task
    JsRelease(task, NULL);
  }

  // If there's still tasks stored in taskQueue
  // then we reschedule a callback to ourselfs
  if(!taskQueue.empty()) {
    uv_async_send(&async);
  }
}

uv_loop_t* uv_chakra_init(JsValueRef globalObject)
{
  uv_loop_t *loop = uv_default_loop();

  uv_async_init(loop, &async, async_callback);

  //uv_pipe_init(loop, &stdin_pipe, 0);
  //uv_pipe_open(&stdin_pipe, 0);
  uv_tty_init(loop, &stdin_pipe, 0, 1);
  uv_tty_set_mode(&stdin_pipe, UV_TTY_MODE_RAW);
  
  //uv_pipe_init(loop, &stdout_pipe, 0);
  //uv_pipe_open(&stdout_pipe, 1);

  create_function(globalObject, "write_async", write_async, &stdout_pipe);
  create_function(globalObject, "read_async", read_async, &stdin_pipe);
  return loop;
}

void uv_chakra_run(uv_loop_t* loop) {
  uv_run(loop, UV_RUN_DEFAULT); 
}


void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

/*
void externalArrayBufferFinalizer(void *data) {
  if(data) {
    free(data);
  } 
}
*/

void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  auto read_req = (read_req_t*) stream->data;
  
  if (nread < 0){
    if (nread == UV_EOF){
        JsValueRef undefined;
        JsValueRef result; 
        JsGetUndefinedValue(&undefined);
        JsValueRef argv[] = {undefined, undefined};
        //JsCallFunction(read_req->reject, argv, 2, &result);
	

	JsCallFunction(read_req->resolve, argv, 2, &result);
	fprintf(stderr, "reset connnection %d", 1);
        //uv_close((uv_handle_t *)&stdin_pipe, NULL);
	uv_tty_init(loop, &stdin_pipe, 0, 1);
	uv_tty_set_mode(&stdin_pipe, UV_TTY_MODE_RAW);
	//uv_pipe_init(loop, &stdin_pipe, 0);
	//uv_pipe_open(&stdin_pipe, 0);

	//uv_close((uv_handle_t *)&stdout_pipe, NULL);
    }
    else {
     fprintf(stderr, "bizarre read %d", nread);
    }
  } else if (nread >= 0) {
    JsValueRef arrayBuffer;
    JsValueRef undefined;
    JsValueRef result; 

    JsGetUndefinedValue(&undefined);

    JsErrorCode error = JsCreateExternalArrayBuffer(buf->base, nread, externalArrayBufferFinalizer, buf->base, &arrayBuffer);

    //fprintf(stderr, "read %d, %d", nread, error);
    JsValueRef argv[] = {undefined, arrayBuffer};
    //fprintf(stderr, "about to call resolve %d", error);
    if(read_req) {
	    error = JsCallFunction(read_req->resolve, argv, 2, &result);
    } else {
	    fprintf(stderr, "read_req is null %d", error);
    }
   // fprintf(stderr, "called resolve %d\n", error);
  } else if (nread == 0) {
     fprintf(stderr, "nul read %d", 1);
  }

  uv_read_stop(stream);
  JsRelease(read_req->resolve, NULL);
  JsRelease(read_req->reject, NULL);
  stream->data = NULL;
  delete read_req;
}
