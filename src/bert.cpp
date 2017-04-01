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

struct val {
  unsigned char type;
  union {
    unsigned char _char;
    double _double;
    int _int;
    long long int _llint;
    const char* _string;
    val* _list;
  } value; 
  
  size_t length;
  val *next; 
  
  inline bool is_prop_list() const 
  {
    //return value._list.length == 1
    //  && value._list.head[0]->type == LIST;
    return type == SMALL_TUPLE  
      && length == 3
      && value._list->type == ATOM
      && value._list->next->type == ATOM
      && value._list->length == 4
      && value._list->next->length == 4
      && strncmp(value._list->value._string, "bert", 4) == 0
      && strncmp(value._list->next->value._string, "dict", 4) == 0
      && value._list->next->next->type == LIST;
  }

  inline val* prop_list() const 
  {
    return value._list->next->next;
    //return value._list;
  }
};
 
template <typename T>
using allocator = MemoryPool<T, 4096>;

template <typename T>
using allocator1 = std::allocator<T>; 

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


std::unordered_map<uint64_t, JsPropertyIdRef> propCache;

void convert_list(JsValueRef *jsArray, val* l) {
  JsCreateArray(l->length, jsArray);
 
  val* current = l->value._list; 
  for(int i = 0; i < l->length; i++) {
    auto jsValue = convert(current);
    JsValueRef index;
    JsIntToNumber(i, &index);
    JsSetIndexedProperty(*jsArray, index, jsValue);
    current = current->next;
  }
}

void convert_prop_list(JsValueRef *jsObj, val* propList) {
  JsCreateObject(jsObj);

  val* currentKvp = propList->value._list; 
  for(int i = 0; i < propList->length; i++) {
    auto propName = currentKvp->value._list;
    auto propVal = currentKvp->value._list->next;

    auto propHash = t1ha0(propName->value._string, propName->length, 12345);
    auto propId = propCache[propHash];

    if(propId == NULL) {
      JsCreatePropertyId(propName->value._string, propName->length, &propId);
      JsAddRef(propId, NULL);
      propCache[propHash] = propId; 
    }
    auto val = convert(propVal);
    JsSetProperty(*jsObj, propId, val, true);
    currentKvp = currentKvp->next;
  }
}

JsValueRef convert(val* v) {
  JsValueRef jsValue;
  JsGetFalseValue(&jsValue);
  
  switch(v->type) {
    case ATOM:
    case BINARY:
    case STRING:
      if(v->length == 0) {
        JsGetFalseValue(&jsValue);
        return jsValue; 
      }

      JsCreateString(v->value._string, v->length, &jsValue);
      break;
    case SMALL_INTEGER: 
      JsIntToNumber(v->value._char, &jsValue);
      break;
    case INTEGER:
      JsIntToNumber(v->value._int, &jsValue);
      break;
    case SMALL_BIG:
      //JsIntToNumber doesn't have enough space to hold small bigs
      //have to pass by doubles ... 
      JsDoubleToNumber(v->value._llint, &jsValue);
      break;
    case NEW_FLOAT: 
      JsDoubleToNumber(v->value._double, &jsValue);
      break;
    case LIST: 
      convert_list(&jsValue, v);
      break;
    case LARGE_TUPLE: 
    case SMALL_TUPLE:
      if(v->is_prop_list()) {
        convert_prop_list(&jsValue, v->prop_list());
      } else {
        convert_list(&jsValue, v);
      }
      break;
    default:
      JsGetFalseValue(&jsValue);
      break;
  }
  return jsValue;
}

static inline int decode_int(const char* buffer,  unsigned int* offset);
static inline unsigned short decode_uint16(const char* buffer,  unsigned int* offset);
static inline unsigned int decode_uint32(const char* buffer,  unsigned int* offset);
static inline double decode_new_float(const char* buffer, unsigned int* offset);
static inline long long int decode_small_big(const char* buffer, unsigned int* offset);
static inline val* decode_string(const char* buffer, unsigned int* offset, size_t length, val* str);
static inline val* decode_list(const char* buffer, unsigned int* offset, size_t length, val* list);


static inline val* decode_intern_(const char* buffer, unsigned int* offset)
{
  auto v = valAlloc.allocate(1);
  return decode_intern_(buffer, offset, v);
} 

static inline val* decode_intern_(const char* buffer, unsigned int* offset, val* v) {
  v->type = decode_small_int(buffer, offset);
  
  switch(v->type) {
    case ATOM:
      decode_string(buffer, offset, decode_uint16(buffer, offset), v);
      break; 
    case BINARY:
      decode_string(buffer, offset, decode_uint32(buffer, offset), v);
      break; 
    case STRING:
      decode_string(buffer, offset, decode_uint16(buffer, offset), v);
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
      decode_list(buffer, offset, decode_int(buffer, offset), v);
      if(decode_small_int(buffer, offset) != NIL) {
        (*offset)--;
      }
      break;
    case SMALL_TUPLE:
      decode_list(buffer, offset, decode_small_int(buffer, offset), v);
      break; 
    case LARGE_TUPLE: 
      decode_list(buffer, offset, decode_int(buffer, offset), v);
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

static inline val* decode_string(const char* buffer, unsigned int* offset, size_t length, val* str) {
  str->length = length; 
  str->value._string = buffer + *offset;

  *offset += length;
  return str; 
}

static inline val* decode_list(const char* buffer, unsigned int* offset, size_t length, val* _list) {
  _list->length = length; 
  _list->value._list = valAlloc.allocate(1);
  
  auto current = _list->value._list; 
  for(int i = 0; i < length; i++) {
    decode_intern_(buffer, offset, current);
    
    if(i < length) {
      current->next = valAlloc.allocate(1);
      current = current->next;
    } else {
      current->next = NULL;
    }
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
