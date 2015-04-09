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
#include <tervel/util/memory/rc/descriptor_util.h>


namespace tervel {
namespace util {

class Descriptor;

namespace memory {
namespace rc {


void ReadFirstOp::help_complete() {
  void *aValue = value_.load();
  while (aValue == nullptr) {
    void *cvalue = address_->load();

    if (is_descriptor_first(cvalue)) {
      tervel::util::Descriptor *descr = unmark_first(cvalue);
      if (tervel::util::memory::rc::watch(descr, address_, cvalue)) {
        cvalue = descr->get_logical_value();
        unwatch(descr);
        value_.compare_exchange_strong(aValue, cvalue);
        return;
      } else {
        util::backoff();
        aValue = value_.load();
      }
    } else {
      value_.compare_exchange_strong(aValue, cvalue);
      return;
    }
  }
}

void *ReadFirstOp::load() {
  return value_.load();
}
}  // namespace rc
}  // namespace memory
}  // namespace util
}  // namespace tervel
