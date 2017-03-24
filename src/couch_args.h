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

#ifndef COUCH_ARGS
#define COUCH_ARGS

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    int          no_eval;
    int          use_http;
    int          use_test_funs;
    int          use_legacy;
    int          use_jasmine;
    int          debug;
    int          stack_size;
    const char** scripts;
    const char*  uri_file;
} couch_args;

couch_args* couch_parse_args(int argc, const char* argv[]);

#ifdef __cplusplus
}
#endif

#endif
