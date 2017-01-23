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

#include "couch_readline.h"
int couch_fgets(char* buf, int size, FILE* fp);
int couch_fgets(char* buf, int size, FILE* fp)
{
    int n, i, c;

    if(size <= 0) return -1;
    n = size - 1;

    for(i = 0; i < n && (c = getc(fp)) != EOF; i++) {
        buf[i] = c;
        if(c == '\n') {
            i++;
            break;
        }
    }

    buf[i] = '\0';
    return i;
}

	
JsValueRef couch_readline(FILE* fp)
{
    char* bytes = NULL;
    char* tmp = NULL;
    size_t used = 0;
    size_t byteslen = 256;
    size_t readlen = 0;

    bytes = (char*)malloc(byteslen * sizeof(char));
    if(bytes == NULL) return NULL;
    
    while((readlen = couch_fgets(bytes+used, byteslen-used, fp)) > 0) {
        used += readlen;
        
        if(bytes[used-1] == '\n') {
            bytes[used-1] = '\0';
            break;
        }
        
        // Double our buffer and read more.
        byteslen *= 2;
        tmp = realloc(bytes, byteslen * sizeof(char));
        if(!tmp) {
            free(bytes);
            return NULL;
        }
        
        bytes = tmp;
    }

    // Shring the buffer to the actual data size
    tmp = realloc(bytes, used * sizeof(char));
    if(!tmp) {
        free(bytes);
        return NULL;
    }
    bytes = tmp;
    byteslen = used;

    JsValueRef str;
    JsCreateString(bytes, byteslen, &str);
    free(bytes);
    return str;
}

