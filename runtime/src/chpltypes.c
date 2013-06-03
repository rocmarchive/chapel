#include "chplrt.h"

#include "chplfp.h"
#include "chpl-mem.h"
#include "chpl-mem-desc.h"
#include "chplcgfns.h"
#include "chpl-comm.h"
#include "chpl-comm-compiler-macros.h"
#include "error.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define NANSTRING "nan"
#define NEGINFSTRING "-inf"
#define POSINFSTRING "inf"

const char* _default_format_write_complex64 = "%g + %gi";
const char* _default_format_write_complex128 = "%g + %gi";

// Uses the system allocator.  Should not be used to create user-visible data
// (error messages are OK).
char* chpl_glom_strings(int numstrings, ...) {
  va_list ap;
  int i, len;
  char* str;

  va_start(ap, numstrings);
  len = 0;
  for (i=0; i<numstrings; i++)
    len += strlen(va_arg(ap, char*));
  va_end(ap);

  str = (char*)chpl_mem_allocMany(len+1, sizeof(char),
                                  CHPL_RT_MD_GLOM_STRINGS_DATA, 0, 0);

  va_start(ap, numstrings);
  str[0] = '\0';
  for (i=0; i<numstrings; i++)
    strcat(str, va_arg(ap, char*));
  va_end(ap);

  return str;
}


chpl_string chpl_format(chpl_string format, ...) {
  va_list ap;
  char z[128];

  va_start(ap, format);
  if (!vsnprintf(z, 127, format, ap))
    chpl_error("overflow encountered in format", 0, 0);
  return string_copy(z, 0, 0);
}


// TODO: This should be placed in a separate file never included in the launcher build.
// Maybe rename chpl-gen-includes and place this in the corresponding C file....
#ifndef LAUNCHER
#include "chpl-gen-includes.h"

struct chpl_chpl____wide_chpl_string_s {
  chpl_localeID_t locale;
  chpl_string addr;
  int64_t size;
};
typedef struct chpl_chpl____wide_chpl_string_s chpl____wide_chpl_string;

chpl_string
chpl_wide_string_copy(chpl____wide_chpl_string* x, int32_t lineno, chpl_string filename) {
  if (x->locale.node == chpl_nodeID)
    return string_copy(x->addr, lineno, filename);
  else {
    chpl_string s;
    chpl_comm_wide_get_string(&s, x,
                              -CHPL_TYPE_chpl_string /* this is unfortunate */,
                              lineno, filename);
    return s;
  }
}

// This copies the remote string data into a local wide string representation
// of the same.
// This routine performs a deep copy of the character array data 
// after fetching the string descriptor from the remote node.  (The char*
// field in the local copy of the remote descriptor has no meaning in the 
// context of the local node, since it refers to elements in the address 
// space on the remote node.)  
// In chpl_comm_wide_get_string() a buffer of the right size is allocated 
// to receive the bytes copied from the remote node.  This buffer will be leaked,
// since no corresponding free is added to the generated code.
void chpl_gen_comm_wide_string_get(void* addr,
  int32_t node, void* raddr, int32_t elemSize, int32_t typeIndex, int32_t len,
  int ln, chpl_string fn)
{
  // This part just copies the descriptor.
  if (chpl_nodeID == node) {
    memcpy(addr, raddr, elemSize*len);
  } else {
#ifdef CHPL_TASK_COMM_GET
    chpl_task_comm_get(addr, node, raddr, elemSize, typeIndex, len, ln, fn);
#else
    chpl_comm_get(addr, node, raddr, elemSize, typeIndex, len, ln, fn);
#endif
  }

  // And now we copy the bytes in the string itself.
  {
    struct chpl_chpl____wide_chpl_string_s* local_str =
      (struct chpl_chpl____wide_chpl_string_s*) addr;
    // Accessing the addr field of the incomplete struct declaration
    // would not work in this context except that this function
    // is always inlined.
    chpl_comm_wide_get_string((chpl_string*) &(local_str->addr),
                              local_str, typeIndex, ln, fn);
    // The bytes live locally, so we have to update the locale.
    local_str->locale = chpl_gen_getLocaleID();
  }
}

// un-macro'd CHPL_WIDEN_STRING
void
chpl_string_widen(chpl____wide_chpl_string* x, chpl_string from)
{
  size_t len = strlen(from) + 1;
  x->locale = chpl_gen_getLocaleID();
  x->addr = chpl_tracked_task_calloc(len, sizeof(char),
                               CHPL_RT_MD_SET_WIDE_STRING, 0, 0);
  strncpy((char*)x->addr, from, len);
  if (*((len-1)+(char*)x->addr) != '\0')
    chpl_internal_error("String missing terminating NUL.");
  x->size = len;    // This size includes the terminating NUL.
}

// un-macro'd CHPL_COMM_WIDE_GET_STRING
void
chpl_comm_wide_get_string(chpl_string* local, struct chpl_chpl____wide_chpl_string_s* x, int32_t tid, int32_t lineno, chpl_string filename)
{
  char* chpl_macro_tmp =
      chpl_tracked_task_calloc(x->size, sizeof(char),
                         CHPL_RT_MD_GET_WIDE_STRING, -1, "<internal>");
    if (chpl_nodeID == x->locale.node)
      memcpy(chpl_macro_tmp, x->addr, x->size);
    else
      chpl_comm_get((void*) &(*chpl_macro_tmp), x->locale.node,
                    (void*)(x->addr),
                    sizeof(char), tid, x->size, lineno, filename);
    *local = chpl_macro_tmp;
}


#endif


//
// We need an allocator for the rest of the code, but for the user
// program it needs to be a locale-aware one with tracking, while for
// the launcher the regular system one will do.
//
static ___always_inline void*
chpltypes_malloc(size_t size, chpl_mem_descInt_t description,
                 int32_t lineno, chpl_string filename) {
#ifndef LAUNCHER
  return chpl_tracked_task_alloc(size, description, lineno, filename);
#else
  return malloc(size);
#endif
}


chpl_string
string_copy(chpl_string x, int32_t lineno, chpl_string filename)
{
  char *z;

  // If the input string is null, just return null.
  if (x == NULL)
    return NULL;

  z = (char*)chpltypes_malloc(strlen(x)+1, CHPL_RT_MD_STRING_COPY_DATA,
                              lineno, filename);
  return strcpy(z, x);
}


chpl_string
string_concat(chpl_string x, chpl_string y, int32_t lineno, chpl_string filename) {
  char *z = (char*)chpltypes_malloc(strlen(x)+strlen(y)+1,
                                    CHPL_RT_MD_STRING_CONCAT_DATA,
                                    lineno, filename);
  z[0] = '\0';
  strcat(z, x);
  strcat(z, y);
  return z;
}


chpl_string
string_strided_select(chpl_string x, int low, int high, int stride, int32_t lineno, chpl_string filename) {
  int64_t length = string_length(x);
  char* result = NULL;
  char* dst = NULL;
  chpl_string src = stride > 0 ? x + low - 1 : x + high - 1;
  int size = high - low >= 0 ? high - low : 0;
  if (low < 1 || low > length || high > length) {
    chpl_error("string index out of bounds", lineno, filename);
  }
  result = chpltypes_malloc(size + 2, CHPL_RT_MD_STRING_STRIDED_SELECT_DATA,
                            lineno, filename);
  dst = result;
  if (stride > 0) {
    while (src - x <= high - 1) {
      *dst++ = *src;
      src += stride;
    }
  } else {
    while (src - x >= low - 1) {
      *dst++ = *src;
      src += stride;
    }
  }
  *dst = '\0';
  // result is already a copy, so we don't have to copy  it again.
  return result;
}

chpl_string
string_select(chpl_string x, int low, int high, int32_t lineno, chpl_string filename) {
  return string_strided_select(x, low, high, 1, lineno, filename);
}

chpl_string
string_index(chpl_string x, int i, int32_t lineno, chpl_string filename) {
  char* buffer = chpltypes_malloc(2, CHPL_RT_MD_STRING_COPY_DATA,
                                  lineno, filename);
  if (i-1 < 0 || i-1 >= string_length(x))
    chpl_error("string index out of bounds", lineno, filename);
  sprintf(buffer, "%c", x[i-1]);
  return buffer;
}


chpl_bool
string_contains(chpl_string x, chpl_string y) {
  if (strstr(x, y))
    return true;
  else
    return false;
}


int32_t chpl_string_compare(chpl_string x, chpl_string y) {
  return (int32_t)strcmp(x, y);
}


int64_t
string_length(chpl_string x) {
  return strlen(x);
}


int64_t real2int( _real64 f) {
  // need to use a union here rather than a pointer cast to avoid gcc
  // warnings when compiling -O3
  union {
    _real64 r;
    uint64_t u;
  } converter;

  converter.r = f;
  return converter.u;
}


int64_t
object2int( _chpl_object o) {
  return (intptr_t) o;
}

const char* chpl_get_argument_i(chpl_main_argument* args, int32_t i)
{
  if( i < 0 ) return NULL;
  if( i > args->argc ) return NULL;
  return args->argv[i];
}

#include "chpl-wide-ptr-fns.h"

// These functions are used by the LLVM wide optimization

// Extract the local address portion of a packed/wide pointer
void* chpl_wide_ptr_get_address_sym(wide_ptr_t ptr);

// Read the locale information from a wide pointer.
void chpl_wide_ptr_read_localeID_sym(wide_ptr_t ptr, chpl_localeID_t* loc);

// Read the node number from a wide pointer.
c_nodeid_t chpl_wide_ptr_get_node_sym(wide_ptr_t ptr);

// Build a wide pointer from locale information and an address.
wide_ptr_t chpl_return_wide_ptr_loc_sym(const chpl_localeID_t* loc, void * addr);

void* chpl_wide_ptr_get_address_sym(wide_ptr_t ptr)
{
  return chpl_wide_ptr_get_address(ptr);
}

void chpl_wide_ptr_read_localeID_sym(wide_ptr_t ptr, chpl_localeID_t* loc)
{
  *loc = chpl_wide_ptr_get_localeID(ptr);
}

c_nodeid_t chpl_wide_ptr_get_node_sym(wide_ptr_t ptr)
{
  return chpl_wide_ptr_get_node(ptr);
}

wide_ptr_t chpl_return_wide_ptr_loc_sym(const chpl_localeID_t* loc, void * addr)
{
  return chpl_return_wide_ptr_loc(*loc, addr);
}


