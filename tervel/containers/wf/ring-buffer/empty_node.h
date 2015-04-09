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
#ifndef TERVEL_WFRB_EMPTYNODE_H_
#define TERVEL_WFRB_EMPTYNODE_H_

#include <tervel/containers/wf/ring-buffer/node.h>


template<class T>
class Node;

namespace tervel {
namespace wf_ring_buffer {
/**
 * TODO(ATB) insert class description
 */
template<class T>
class EmptyNode : public Node<T> {
 public:
  explicit EmptyNode<T>(long seq)
      : Node<T>(reinterpret_cast<T>(nullptr), seq) {}

  ~EmptyNode<T>() {}

  // REVIEW(steven) missing description
  bool is_EmptyNode() { return true; }

  // REVIEW(steven) missing description
  bool is_ElemNode() { return false; }
};  // EmptyNode class

}  // namespace wf_ring_buffer
}  // namespace tervel
#endif  // TERVEL_WFRB_EMPTYNODE_H_
