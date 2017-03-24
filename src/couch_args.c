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

#include <stdlib.h>
#include <string.h>

#include "couch_args.h"
#include "help.h"

couch_args*
couch_parse_args(int argc, const char* argv[])
{
    couch_args* args;
    int i = 1;

    args = (couch_args*) malloc(sizeof(couch_args));
    if(args == NULL)
        return NULL;

    memset(args, '\0', sizeof(couch_args));
    args->stack_size = 64L * 1024L * 1024L;

    while(i < argc) {
        if(strcmp("-h", argv[i]) == 0) {
            DISPLAY_USAGE;
            exit(0);
        } else if(strcmp("-V", argv[i]) == 0) {
            DISPLAY_VERSION;
            exit(0);
        } else if(strcmp("-H", argv[i]) == 0) {
            args->use_http = 1;
        } else if(strcmp("-T", argv[i]) == 0) {
            args->use_test_funs = 1;
        } else if(strcmp("-L", argv[i]) == 0) {
            args->use_legacy = 1;
        } else if(strcmp("-e", argv[i]) == 0) {
            args->use_evented = 1;
        } else if(strcmp("-j", argv[i]) == 0) {
            args->use_jasmine = 1;
        } else if(strcmp("-d", argv[i]) == 0) {
            args->debug= 1;
        } else if(strcmp("-S", argv[i]) == 0) {
            args->stack_size = atoi(argv[++i]);
            if(args->stack_size <= 0) {
                fprintf(stderr, "Invalid stack size.\n");
                exit(2);
            }
        } else if(strcmp("-u", argv[i]) == 0) {
            args->uri_file = argv[++i];
        } else if(strcmp("--no-eval", argv[i]) == 0) {
            args->no_eval = 1;
        } else if(strcmp("--", argv[i]) == 0) {
            i++;
            break;
        } else {
            break;
        }
        i++;
    }

    if(i >= argc) {
        DISPLAY_USAGE;
        exit(3);
    }
    args->scripts = argv + i;

    return args;
}


