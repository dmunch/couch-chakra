#include "couch_chakra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unordered_map>
#include <vector>
#include <memory>
#include "t1ha.h"
#include <netinet/in.h>

#include "MemoryPool.h"

const unsigned char BERT_START = 131;
const unsigned char SMALL_ATOM = 115;
const unsigned char ATOM = 100;
const unsigned char BINARY = 109;
const unsigned char SMALL_INTEGER = 97;
const unsigned char INTEGER = 98;
const unsigned char SMALL_BIG = 110;
const unsigned char LARGE_BIG = 111;
const unsigned char FLOAT = 99;
const unsigned char NEW_FLOAT = 70;
const unsigned char STRING = 107;
const unsigned char LIST = 108;
const unsigned char SMALL_TUPLE = 104;
const unsigned char LARGE_TUPLE = 105;
const unsigned char NIL = 106;
const unsigned char ZERO = 0;
const unsigned char ZERO_CHAR = 48;


struct val;

struct shared_string {
  const char* head;
  size_t length;
  bool operator==(const shared_string&other) const
  {
    return other.length == length && strncmp(other.head, head, length) == 0; 
  }
};

namespace std {
template <>
struct std::hash<shared_string>
{
  std::size_t operator()(const shared_string& k) const
  {
    return t1ha0(k.head, k.length, 12345);
  }
};
}

struct list {
  val** head;
  size_t length;
};

struct val {
  unsigned char type;
  union {
    unsigned char _char;
    double _double;
    int _int;
    long long int _llint;
    shared_string _string;
    list _list;
  } value; 
  
  inline bool is_prop_list() const 
  {
    //return value._list.length == 1
    //  && value._list.head[0]->type == LIST;
    return type == SMALL_TUPLE  
      && value._list.length == 3
      && value._list.head[0]->type == ATOM
      && value._list.head[1]->type == ATOM
      && value._list.head[0]->value._string.length == 4
      && value._list.head[1]->value._string.length == 4
      && strncmp(value._list.head[0]->value._string.head, "bert", 4) == 0
      && strncmp(value._list.head[1]->value._string.head, "dict", 4) == 0
      && value._list.head[2]->type == LIST;
  }

  inline list* prop_list() const 
  {
    //return value._list.head[0]->value._list;
    return &value._list.head[2]->value._list;
  }
};

/*
std::allocator<val> valAlloc;
std::allocator<val*> valPtrAlloc;
std::allocator<shared_string> strAlloc;
std::allocator<list> listAlloc;
*/
 
template <typename T>
using allocator = MemoryPool<T, 4096>;

allocator<val> valAlloc;

static inline val* decode_intern_(const char* buffer, unsigned int* offset);
static inline val* decode_intern_(const char* buffer, unsigned int* offset, val * v);

static inline unsigned char decode_small_int(const char* buffer,  unsigned int* offset);
JsValueRef convert(val* v); 

JsValueRef decode(const char* buffer) {
  unsigned int offset = 0;
  auto start = decode_small_int(buffer, &offset);

  if(start != BERT_START) {
    JsValueRef falseValue;
    JsGetFalseValue(&falseValue);
  
    return falseValue;
  }
  auto v = decode_intern_(buffer, &offset);
  return convert(v);
}

JS_FUN_DEF(Bert_decode)
{
  BYTE *arrayBufferStorage;
  unsigned int arrayBufferSize;

  JsGetArrayBufferStorage(argv[1], &arrayBufferStorage, &arrayBufferSize);
  //JsGetDataViewStorage(argv[1], &arrayBufferStorage, &arrayBufferSize);
  
  if(arrayBufferSize == 0) {
    JsValueRef falseValue;
    JsGetTrueValue(&falseValue);
    return falseValue;
  }
  return decode((const char *) arrayBufferStorage);
}

JS_FUN_DEF(BertConstructor) 
{
  create_function(argv[0], "decode", Bert_decode, NULL); 
  return argv[0];
}


std::unordered_map<uint64_t, JsPropertyIdRef> propCache1;
std::unordered_map<shared_string, JsPropertyIdRef> propCache;
std::vector<JsValueRef> indexVector;


JsValueRef convert_list(list* l) {
  JsValueRef jsArray;
  JsCreateArray(l->length, &jsArray);
  
  for(int i = 0; i < l->length; i++) {
    auto jsValue = convert(l->head[i]);
    JsValueRef index;
    JsIntToNumber(i, &index);
    JsSetIndexedProperty(jsArray, index, jsValue);
  }
  return jsArray;
}

JsValueRef convert_prop_list(list* propList) {
  JsValueRef value;
  JsCreateObject(&value);

  for(int i = 0; i < propList->length; i++) {
    auto kvp = &propList->head[i]->value._list;  
    auto propName = &kvp->head[0]->value._string;
    auto propVal = kvp->head[1];

    auto propHash = t1ha0(propName->head, propName->length, 12345);
    auto propId = propCache1[propHash];
    //auto propId = propCache[*propName];

    if(propId == NULL) {
      JsCreatePropertyId(propName->head, propName->length, &propId);
      JsAddRef(propId, NULL);
      propCache1[propHash] = propId; 
      //propCache[*propName] = propId; 
    }
    auto val = convert(propVal);
    JsSetProperty(value, propId, val, true);
  }
  return value;
}

JsValueRef convert(val* v) {
  switch(v->type) {
    case ATOM:
    case BINARY:
    case STRING:
               {
                 auto str = &v->value._string;
                 JsValueRef jsValue;
                 
                 if(str->length == 0) {
                  JsGetFalseValue(&jsValue);
                  return jsValue; 
                 }
                 
                 JsCreateString(str->head, str->length, &jsValue);
                 //delete str; 
                 return jsValue;
               }
    case SMALL_INTEGER: 
               {
                 JsValueRef jsValue;
                 JsIntToNumber(v->value._char, &jsValue);
                 return jsValue;
               }
    case INTEGER:
               {
                 JsValueRef jsValue;
                 JsIntToNumber(v->value._int, &jsValue);
                 return jsValue;
               }
    case SMALL_BIG:
               {
                 JsValueRef jsValue;
                 
                 //JsIntToNumber doesn't have enough space to hold small bigs
                 //have to pass by doubles ... 
                 double dVal = v->value._llint;
                 JsDoubleToNumber(dVal, &jsValue);
                 return jsValue;
               }
    case NEW_FLOAT: 
               {
                 JsValueRef jsValue;
                 JsDoubleToNumber(v->value._double, &jsValue);
                 return jsValue;
               }
    case LIST: return convert_list(&v->value._list);
    case LARGE_TUPLE: 
    case SMALL_TUPLE:
               if(v->is_prop_list()) {
                 return convert_prop_list(v->prop_list());
               } else {
                 return convert_list(&v->value._list);
               }
    default : {
                JsValueRef falseValue;
                JsGetFalseValue(&falseValue);

                return falseValue;
              }
  }
  return NULL;
}

static inline int decode_int(const char* buffer,  unsigned int* offset);
static inline unsigned short decode_uint16(const char* buffer,  unsigned int* offset);
static inline unsigned int decode_uint32(const char* buffer,  unsigned int* offset);
static inline double decode_new_float(const char* buffer, unsigned int* offset);
static inline long long int decode_small_big(const char* buffer, unsigned int* offset);
static inline shared_string* decode_string(const char* buffer, unsigned int* offset, size_t length, shared_string* str);
static inline list* decode_list(const char* buffer, unsigned int* offset, size_t length, list* list);


static inline val* decode_intern_(const char* buffer, unsigned int* offset)
{
  auto v = valAlloc.allocate();
  return decode_intern_(buffer, offset, v);
} 

static inline val* decode_intern_(const char* buffer, unsigned int* offset, val* v) {
  v->type = decode_small_int(buffer, offset);
  
  switch(v->type) {
    case ATOM:
      decode_string(buffer, offset, decode_uint16(buffer, offset), &v->value._string);
      break; 
    case BINARY:
      decode_string(buffer, offset, decode_uint32(buffer, offset), &v->value._string);
      break; 
    case STRING:
      decode_string(buffer, offset, decode_uint16(buffer, offset), &v->value._string);
      break; 
    case SMALL_INTEGER: 
      v->value._char = decode_small_int(buffer, offset); 
      break; 
    case INTEGER:
      v->value._int = decode_int(buffer, offset); 
      break; 
    case SMALL_BIG:
      v->value._llint = decode_small_big(buffer, offset);
      break; 
    case NEW_FLOAT: 
      v->value._double = decode_new_float(buffer, offset); 
      break; 
    case LIST:
      decode_list(buffer, offset, decode_int(buffer, offset), &v->value._list);
      if(decode_small_int(buffer, offset) != NIL) {
        (*offset)--;
      }
      break;
    case SMALL_TUPLE:
      decode_list(buffer, offset, decode_small_int(buffer, offset), &v->value._list);
      break; 
    case LARGE_TUPLE: 
      decode_list(buffer, offset, decode_int(buffer, offset), &v->value._list);
      break; 
    default :
     break; 
  }
  return v;
}


static inline unsigned char decode_small_int(const char* buffer, unsigned int* offset) {
  return buffer[(*offset)++]; 
}

static inline unsigned short decode_uint16(const char* buffer, unsigned int* offset) {
  auto value =  *((unsigned short*) (buffer + *offset));
  *offset += 2;
  return ntohs(value);
}

static inline unsigned int decode_uint32(const char* buffer, unsigned int* offset) {
  auto value =  *((unsigned int*) (buffer + *offset));
  *offset += 4;
  return ntohl(value);
}

static inline int decode_int(const char* buffer, unsigned int* offset) {
  auto value =  *((int*) (buffer + *offset));
  *offset += 4;
  return ntohl(value);
}

static inline double decode_new_float(const char* buffer, unsigned int* offset) {
  const char b[] = {
    buffer[*offset + 7],
    buffer[*offset + 6],
    buffer[*offset + 5],
    buffer[*offset + 4],
    buffer[*offset + 3],
    buffer[*offset + 2],
    buffer[*offset + 1],
    buffer[*offset + 0]
  };
  
  //auto value =  *((double*) (buffer + *offset));
  auto value =  *((double*) (b));
  *offset += 8;
  return value;
}

static inline shared_string* decode_string(const char* buffer, unsigned int* offset, size_t length, shared_string *str) {
  str->length = length; 
  str->head = buffer + *offset;

  *offset += length;
  return str; 
}

static inline list* decode_list(const char* buffer, unsigned int* offset, size_t length, list* _list) {
  _list->length = length; 
  _list->head = (val**) malloc(sizeof(val*) * length);
  
  for(int i = 0; i < length; i++) {
    *(_list->head + i) = decode_intern_(buffer, offset);
  }
  return _list;
}


unsigned long long int powers[] = {
  (unsigned long long int) 1,
  (unsigned long long int) 256,
  (unsigned long long int) 256 * 256,
  (unsigned long long int) 256 * 256 * 256, 
  (unsigned long long int) 256 * 256 * 256 * 256, 
  (unsigned long long int) 256 * 256 * 256 * 256 * 256, 
  (unsigned long long int) 256 * 256 * 256 * 256 * 256 * 256, 
  (unsigned long long int) 256 * 256 * 256 * 256 * 256 * 256 * 256
  //afterwards JavaScripts number can't hold the value anyway..
};

static inline long long int decode_small_big(const char* buffer, unsigned int* offset) {
  auto length = decode_small_int(buffer, offset);
  auto sign = decode_small_int(buffer, offset);
 
  long long int value = 0;
  
  #pragma clang loop vectorize(enable)
  for(int i = 0 ; i < length; i++) {
    value += ((unsigned char)buffer[(*offset) + i]) * powers[i];
  } 
  *offset += length;
  return value;
}
