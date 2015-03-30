#ifndef TERVEL_MEMORY_HP_HAZARD_POINTER_H_
#define TERVEL_MEMORY_HP_HAZARD_POINTER_H_

#include <atomic>
#include <utility>

#include <assert.h>
#include <stddef.h>
#include <memory>
#include <cstdint>

#include "tervel/util/info.h"
#include "tervel/util/util.h"
#include "tervel/util/memory/hp/list_manager.h"

namespace tervel {
namespace util {
namespace memory {
namespace hp {

class Element;

/**
 * This class is used to maintain the list of hazard pointed objects.
 * Any value can be written into a slot, however we provide special
 * implementation for Elements, in that we call their on_* functions.
 * This allows for more expressive operations to be performed.
 *
 * If an individual thread requires more than one element to be hazard pointer
 * protected at a single instance, then SlotIDs should be added.
 */
class HazardPointer {
 public:
  enum class SlotID : size_t {SHORTUSE = 0 , PROG_ASSUR = 1, END = 2};

  explicit HazardPointer(int num_threads)
      // The total number of slots needed is equal to the number of threads
      // multiples by the number of slots used.
      // Do to the potential of reordering, num_slots_ can not be used to
      // initialize watches.
      : hp_list_manager_(num_threads)
      , watches_(new std::atomic<void *>[num_threads *
            static_cast<size_t>(SlotID::END)])
      , num_slots_ {num_threads * static_cast<size_t>(SlotID::END)} {}

  ~HazardPointer() {
    for (int i = 0; i < num_slots_; i++) {
      assert(watches_[i].load() != nullptr && "Some memory is still being watched and hazard pointer construct has been destroyed");
    }
    // delete watches_; // std::unique_ptr causes this array to be destroyed
  }


  // -------
  // Static Functions
  // -------

  /**
   * This method is used to achieve a hazard pointer watch on the the based descr.
   * Internally it will call the descriptors on_watch function.
   *
   * If after writing descr the object is still at the address
   * (indicated by *a == value), it will call on_watch.
   * If that returns true then it will return true.
   * Otherwise it removes the hazard pointer watch and returns false
   *
   * @param slot The position to place the descr value in the watch table.
   * @param descr The descr that is to be watched.
   * @param address The address to check
   * @param expected The value which is to be expected at the address
   */
  static bool watch(SlotID slot_id, Element *elem, std::atomic<void *> *address,
        void *expected, HazardPointer *hazard_pointer =
        tervel::tl_thread_info->get_hazard_pointer());

  /**
   * This method is used to achieve a hazard pointer watch on a memory address.
   *
   * If after writing the value, it is still at the address
   * (indicated by *a == value), will return true.
   * Otherwise it removes the hazard pointer watch and returns false
   *
   * @param slot The position to place the value in the watch table.
   * @param value The value that is to be watched.
   * @param address The address to check
   * @param expected The value which is to be expected at the address
   */
  static bool watch(SlotID slot_id, void *value, std::atomic<void *> *address
      , void *expected, HazardPointer *hazard_pointer =
      tervel::tl_thread_info->get_hazard_pointer());

  /**
   * This method is used to remove the hazard pointer watch.
   * If a descr is passed then it will internally call descr->on_unwatch.
   *
   * @param slot the slot to remove the watch
   */
  static void unwatch(SlotID slot_id, HazardPointer *hazard_pointer =
        tervel::tl_thread_info->get_hazard_pointer());

  /**
   * This method is used to remove the hazard pointer watch.
   * If a descr is passed then it will internally call descr->on_unwatch.
   *
   * @param slot the slot to remove the watch
   * @param descr to call on_unwatch on.
   */
  static void unwatch(SlotID slot_id, Element *descr,
          HazardPointer *hazard_pointer =
          tervel::tl_thread_info->get_hazard_pointer());

  /**
   * This method is used to determine if a hazard pointer watch exists on a
   * passed value.
   * If a descr is passed then it will internally call descr->on_is_watched.
   *
   * @param descr to call on_is_watched on.
   */
  static bool is_watched(Element *descr, HazardPointer *hazard_pointer =
        tervel::tl_thread_info->get_hazard_pointer());

  /**
   * This method is used to determine if a hazard pointer watch exists on a
   * passed value.
   * If a descr is passed then it will internally call descr->on_is_watched.
   *
   * @param value to check if watch
   */
  static bool is_watched(void *value, HazardPointer *hazard_pointer =
        tervel::tl_thread_info->get_hazard_pointer());


  // -------
  // Member Functions
  // -------
  /**
   * This function takes a SlotID and stores the specified value into that
   * the threads alloated slot for that id in the hazard pointer watch list
   *
   * @param slot The id of the slot to watch.
   * @param value The value to watch
   **/
  void watch(SlotID slot, void *value) {
    // REVIEW(carlos): enum classes don't support automatic casting to int (the
    //   return type of get_slot). Don't assign the result of get_slot to slot,
    //   and just use the returned value directly in the operator[] of watches_:
    //     watches_[get_slot(slot)].store(value);
    size_t slot_pos = get_slot(slot);
    watches_[slot_pos].store(value);
  }

  /**
   * This function takes a SlotID and stores null into that
   * the threads alloated slot for that id in the hazard pointer watch list
   *
   * @param slot The id of the slot to watch.
   */
  void clear_watch(SlotID slot) {
    size_t slot_pos = get_slot(slot);
    watches_[slot_pos].store(nullptr);
  }


  /**
   * This function returns true of the specified value is being watched.
   *
   * @param value The value to check.
   * @return true is the table contains the specified value
   */
  bool contains(void *value) {
    for (size_t i = 0; i < num_slots_; i++) {
      if (watches_[i].load() == value) {
        return true;
      }
    }
    return false;
  }

 public:
  // Shared HP Element list manager
  util::memory::hp::ListManager hp_list_manager_;


 private:
  /**
   * This function calculates a the position of a threads slot for the
   * specified SlotID
   *
   * @param slot The slot id to get the position of
   */
  size_t get_slot(SlotID id) {
    size_t s = static_cast<size_t>(id) + (static_cast<size_t>(SlotID::END) *
          tervel::tl_thread_info->get_thread_id());
    assert(s < num_slots_);
    return s;
  }

  std::unique_ptr<std::atomic<void *>[]> watches_;
  const size_t num_slots_;

  DISALLOW_COPY_AND_ASSIGN(HazardPointer);
};  // HazardPointer


}  // namespace hp
}  // namespace memory
}  // namepsace UTIL
}  // namespace tervel

#endif  // TERVEL_MEMORY_HP_HAZARD_POINTER_H_
