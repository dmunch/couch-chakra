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
#include <string.h>
#include <stdlib.h>
#include "couch_readfile.h"

size_t slurp_file(const char* file, char** outbuf_p)
{
    FILE* fp;
    char fbuf[16384];
    char *buf = NULL;
    char* tmp;
    size_t nread = 0;
    size_t buflen = 0;

    if(strcmp(file, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(file, "r");
        if(fp == NULL) {
            fprintf(stderr, "Failed to read file: %s\n", file);
            exit(3);
        }
    }

    while((nread = fread(fbuf, 1, 16384, fp)) > 0) {
        if(buf == NULL) {
            buf = (char*) malloc(nread + 1);
            if(buf == NULL) {
                fprintf(stderr, "Out of memory.\n");
                exit(3);
            }
            memcpy(buf, fbuf, nread);
        } else {
            tmp = (char*) malloc(buflen + nread + 1);
            if(tmp == NULL) {
                fprintf(stderr, "Out of memory.\n");
                exit(3);
            }
            memcpy(tmp, buf, buflen);
            memcpy(tmp+buflen, fbuf, nread);
            free(buf);
            buf = tmp;
        }
        buflen += nread;
        buf[buflen] = '\0';
    }
    *outbuf_p = buf;
    return buflen + 1;
}


