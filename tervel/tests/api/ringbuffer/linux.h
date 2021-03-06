/*
#The MIT License (MIT)
#
#Copyright (c) 2015 University of Central Florida's Computer Software Engineering
#Scalable & Secure Systems (CSE - S3) Lab
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in
#all copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#THE SOFTWARE.
#
*/

#ifndef DS_API_H_
#define DS_API_H_


#include <string>
#include <atomic>
#include "linux_buffer/lockfree_rb_q.cc"

typedef unsigned char Value;

typedef  LockFreeQueue<Value> container_t;

#include "../../src/main.h"

DEFINE_int32(prefill, 0, "The number elements to place in the buffer on init.");
DEFINE_int32(capacity, 32768, "The capacity of the buffer.");

#define DS_DECLARE_CODE \
  container_t *container; \
  std::atomic<size_t> thread_id_counter {0}; \

#define DS_DESTORY_CODE

#define DS_ATTACH_THREAD \
  size_t temp = thread_id_counter.fetch_add(1); \
  set_thr_id(temp); \

#define DS_DETACH_THREAD

#define DS_INIT_CODE \
DS_ATTACH_THREAD \
container = new container_t(FLAGS_num_threads+1, FLAGS_num_threads+1, FLAGS_capacity); \
\
Value x = 1; \
for (int i = 0; i < FLAGS_prefill; i++) { \
  container->push(x++); \
  if (x == 0) x = 1; \
} \

#define DS_NAME "Linux"

#define DS_CONFIG_STR \
   "\n" _DS_CONFIG_INDENT "prefill : " + std::to_string(FLAGS_prefill) +"" + \
   "\n" _DS_CONFIG_INDENT "capacity : " + std::to_string(FLAGS_capacity) +""

#define DS_STATE_STR " "

#define OP_RAND \
  /* std::uniform_int_distribution<Value> random(1, UINT_MAX); */ \
  int ecount = 1;


#define OP_CODE \
  MACRO_OP_MAKER(0, { \
    Value value = ecount++;\
    if (ecount == 0) ecount++; \
    container->push(value); \
  } \
  ) \
 MACRO_OP_MAKER(1, { \
      Value value = 0; \
      value = container->pop(); \
      opRes = (value != 0); \
    } \
  )

#define DS_OP_NAMES "enqueue", "dequeue"

#define DS_OP_COUNT 2

#define DS_EXTRA_END_SIGNAL {\
 for (size_t i = 0; i < FLAGS_num_threads; i++) {\
  container->push(1); \
 }\
}\

inline void sanity_check(container_t *container) {};

#endif  // DS_API_H_

