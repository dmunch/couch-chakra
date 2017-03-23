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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chrono>
#include <unordered_map>
#include <string>

JS_FUN_DEF(log);
JS_FUN_DEF(time);
JS_FUN_DEF(timeEnd);


struct PrintOptions {
  inline PrintOptions(FILE* f, bool nl) : file(f), newLine(nl) {}
  
  FILE* file;
  bool newLine;
};

void chakra_console_init(JsValueRef context) {
  JsPropertyIdRef moduleId; 
  JsCreatePropertyId("console", strlen("console"), &moduleId);


  JsValueRef module;
  JsCreateObject(&module);

  JsSetProperty(context, moduleId, module, true);
  create_function(module, "log", log, new PrintOptions(stderr, true));
  create_function(module, "time", time, NULL);
  create_function(module, "timeEnd", timeEnd, NULL);
}


const char* to_str(JsValueRef strValue) {
  size_t written;
  size_t bufferSize;
  JsCopyString(strValue, NULL, 0, &bufferSize);
  
  if(bufferSize < 1) {
    return NULL;
  } 

  char *str = (char *) malloc(bufferSize + 1);
  JsCopyString(strValue, str, bufferSize, &written);
  str[bufferSize] = 0;

  return str;
} 

JS_FUN_DEF(log)
{
  auto options = (PrintOptions*) callbackState;
  
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  for(int a = 1; a < argc; a++){
    JsValueRef value = argv[a];

    if(value == JS_INVALID_REFERENCE) {
      continue;
    }

    JsValueType type;
    JsGetValueType(value, &type);
    
    if(type == JsUndefined) {
      fprintf(options->file, "%s ", "undefined");
      continue;
    }
   
    JsValueRef strValue;
    JsConvertValueToString(value, &strValue);

    const char* str = to_str(strValue);
    fprintf(options->file, "%s", str);
    free((void*)str);
  }

  if(options->newLine) {
    fputc('\n', options->file);
    fflush(options->file);
  }

  return trueValue;
}

typedef std::chrono::high_resolution_clock hr_clock;
typedef std::chrono::milliseconds milliseconds;
std::unordered_map<std::string, hr_clock::time_point> timerLabels;

JS_FUN_DEF(time) {
  JsValueRef label = argv[1];

  const char* str = to_str(label);
  timerLabels[std::string(str)] = hr_clock::now();
  free((void*)str);
  
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
  return trueValue;
}

JS_FUN_DEF(timeEnd) {
  JsValueRef label = argv[1];

  const char* str = to_str(label);
  auto time_point = timerLabels[std::string(str)];
  
  auto elapsed = std::chrono::duration_cast<milliseconds>(hr_clock::now() - time_point); 
  fprintf(stderr, "%s elapsed: %lld ms\n", str, elapsed.count());
  free((void*)str);
  
  JsValueRef trueValue;
  JsGetTrueValue(&trueValue);
 
  return trueValue;
}
