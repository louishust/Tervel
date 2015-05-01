/*
The MIT License (MIT)

Copyright (c) 2015 University of Central Florida's Computer Software Engineering
Scalable & Secure Systems (CSE - S3) Lab

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef TERVEL_CONTAINERS_WF_RINGBUFFER_RINGBUFFER_OPREC_H_
#define TERVEL_CONTAINERS_WF_RINGBUFFER_RINGBUFFER_OPREC_H_

#include <atomic>
#include <assert.h>
#include <cstddef>
#include <memory>
#include <thread>
#include <string>

#include <tervel/util/info.h>
#include <tervel/util/descriptor.h>
#include <tervel/util/progress_assurance.h>
#include <tervel/util/memory/hp/hazard_pointer.h>
#include <tervel/util/memory/rc/descriptor_util.h>

namespace tervel {
namespace containers {
namespace lf {

template<typename T>
class RingBuffer<T>::BufferOp{
 public:
  BufferOp(RingBuffer<T> *rb)
    : rb_(rb) {}
  ~BufferOp() {
    Helper *h = helper_.load();
    if (h != nullptr && h != BufferOp::fail) {
      delete h;
    }
  };

  bool associate(Helper *h) {
    Helper *temp = nullptr;
    bool res = helper_.compare_exchange_strong(temp, h);
    if (res || temp == nullptr) { // success
      return true;
    } else { // fail
      return false;
    }
  }

  bool valid(Helper * h) {
    return helper_.load() == h;
  }

  void fail() {
    Helper *temp = nullptr;
    helper_.compare_exchange_strong(temp, fail_val_);
  }

  bool isFail(Helper * &h) {
    return (h=helper_.load()) == fail_val_;
  }

  Helper * getHelper() {
    return helper_.load();
  }

  void notDone() {
    return helper_.load() == nullptr;
  }

  // on_is_watched() function is not needed because helpers
  // are removed before removing the watch on the op record

 private:
  const Helper * fail_val_{static_cast<Helper *>(0x1)};
  const RingBuffer<T> *rb_;
  std::atomic<Helper *> helper_{nullptr};
};

}  // namespace wf
}  // namespace containers
}  // namespace tervel
#endif  // TERVEL_CONTAINERS_WF_RINGBUFFER_RINGBUFFER_OPREC_H_

#include <tervel/containers/lf/ring-buffer/buffer_help.h>
#include <tervel/containers/lf/ring-buffer/enqueue_op.h>
#include <tervel/containers/lf/ring-buffer/dequeue_op.h>

