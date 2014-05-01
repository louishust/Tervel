#ifndef TERVEL_UTIL_MEMORY_HP_LIST_MANAGER_H_
#define TERVEL_UTIL_MEMORY_HP_LIST_MANAGER_H_

#include <atomic>
#include <memory>
#include <utility>

#include <assert.h>
#include <stdint.h>

#include "tervel/util/system.h"
#include "tervel/util/memory/hp/hp_element.h"
#include "tervel/util/memory/hp/hp_list.h"

namespace tervel {
namespace util {
namespace memory {
namespace hp {

class ElementList;
class Element;

/**
 * Encapsulates a shared central 'to free list' between several thread-local
 * lists. When a thread is destroyed it will send its unfreeable items to this
 * list, which is freed by the user.
 */
class ListManager {
 public:
  explicit ListManager(size_t number_pools)
      : free_lists_(new ManagedPool[number_pools])
      , number_pools_(number_pools) {}

  ~ListManager() {
    // TODO(steven): destroy pool by freeing each Element
  }

  ElementList * allocate_list() {
    return new ElementList(this);
  }

 private:
  struct ManagedPool {
    // std::unique_ptr<ElementList> pool {nullptr};
    Element * element_list_ {nullptr};
    // char padding[CACHE_LINE_SIZE - sizeof(element_list_)];
  };

  void recieve_element_list(uint64_t tid, Element * element_list) {
    free_lists_[tid].element_list_ = element_list;
  };

  std::unique_ptr<ManagedPool[]> free_lists_;
  size_t number_pools_;

  friend class ElementList;
  DISALLOW_COPY_AND_ASSIGN(ListManager);
};

}  // namespace hp
}  // namespace memory
}  // namespace util
}  // namespace tervel
#endif  // TERVEL_UTIL_MEMORY_HP_LIST_MANAGER_H_
