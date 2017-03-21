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

#include "couch_chakra.h"
#include "uv_chakra.h"
#include <stdlib.h>
#include <queue>

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void promiseContinuationCallback(JsValueRef task, void *callbackState);

uv_pipe_t stdin_pipe;
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

  uv_stream_t* in_stream = (uv_stream_t*) callbackState;
  in_stream->data = read_req;  
  uv_read_start(in_stream, alloc_buffer, read_stdin);
  
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  return trueValue;
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

  JsSetPromiseContinuationCallback(promiseContinuationCallback, NULL);
  uv_async_init(loop, &async, async_callback);

  uv_pipe_init(loop, &stdin_pipe, 0);
  uv_pipe_open(&stdin_pipe, 0);
  
  uv_pipe_init(loop, &stdout_pipe, 0);
  uv_pipe_open(&stdout_pipe, 1);

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

void bufferFinalizer(void *data) {
  if(data) {
    free(data);
  } 
}

void read_stdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  auto read_req = (read_req_t*) stream->data;
  
  if (nread < 0){
    if (nread == UV_EOF){
        JsValueRef undefined;
        JsValueRef result; 
        JsGetUndefinedValue(&undefined);
        JsValueRef argv[] = {undefined, undefined};
        JsCallFunction(read_req->reject, argv, 2, &result);
        //uv_close((uv_handle_t *)&stdin_pipe, NULL);
        //uv_close((uv_handle_t *)&stdout_pipe, NULL);
    }
  } else if (nread > 0) {
    JsValueRef arrayBuffer;
    JsValueRef undefined;
    JsValueRef result; 

    JsGetUndefinedValue(&undefined);

    JsCreateExternalArrayBuffer(buf->base, nread, bufferFinalizer, buf->base, &arrayBuffer);

    JsValueRef argv[] = {undefined, arrayBuffer};
    JsCallFunction(read_req->resolve, argv, 2, &result);
  }

  stream->data = NULL;
  delete read_req;
}
