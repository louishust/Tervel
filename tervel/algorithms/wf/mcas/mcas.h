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
#ifndef TERVEL_MCAS_MCAS_H_
#define TERVEL_MCAS_MCAS_H_

#include <tervel/algorithms/wf/mcas/mcas_helper.h>
#include <tervel/algorithms/wf/mcas/mcas_casrow.h>
#include <tervel/util/info.h>
#include <tervel/util/progress_assurance.h>
#include <tervel/util/memory/rc/descriptor_util.h>
#include <cstdint>
#include <algorithm>

namespace tervel {
namespace algorithms {
namespace wf {
namespace mcas {
/**
 * This function determines the logical value of the address.
 *
 * @param address the location to determine the value
 * @return the logical value
 */
template<class T>
inline T read(std::atomic<T> *address) {
  void *value = util::memory::rc::descriptor_read_first(address);
  return reinterpret_cast<T>(value);
}


// REVIEW(carlos): class does not follow class naming conventions
// RESPONSE(steven): Should it be MultiWordCompareAndSwap instead?
/**
 * This is the MCAS class, it is used to perform a Multi-Word Compare-and-Swap
 * To execute an MCAS, simply call addCASTriple for each address you want to
 * update, then call execute();
 * This function is wait-free.
 */
template<class T>
class MCAS : public util::OpRecord {
 public:
  static constexpr void * MCAS_FAIL_CONST = reinterpret_cast<void *>(0x1L);

  explicit MCAS<T>(int max_rows)
      : cas_rows_(new CasRow<T>[max_rows])
      , max_rows_ {max_rows} {}

  ~MCAS<T>() {
    state_.store(MCasState::DELETED);
    for (int i = 0; i < row_count_; i++) {
      Helper<T>* helper = cas_rows_[i].helper_.load();
      // The No check flag is true because each was check prior
      // to the call of this descructor.
      if (helper == reinterpret_cast<Helper<T> *>(MCAS_FAIL_CONST)) {
        break;
      }
      util::memory::rc::free_descriptor(helper, true);
    }
  }

  /**
   * This function is used to add a CAS triple to the MCAS operation.
   * Each Triple consistens of an address to replace the expected value with
   * a new value iff each other address holds its expected value.
   *
   * This function returns false in the event the address already exists in
   * the mcas operation or the passed values are not valid. A Valid value
   * is a value which does not use one of the reserved bits or constants.
   * See related documentation for more details
   *
   * @param address
   * @param expected_value
   * @param new_value
   * @return true if successfully added.
   */
  bool add_cas_triple(std::atomic<T> *address, T expected_value, T new_value);

  /**
   * This function is called after all the CAS triples have been added to the
   * operation. It will attempt to apply the operation.
   *
   * @returns true if it replaced the values at each address with a new value
   */
  bool execute();

  /**
   * THis function overrides the virtual function in the OpRecord class
   * It is called by the progress aurrance scheme. Upon its return the MCAS
   * operation must be completed
   */
  void help_complete() {
    mcas_complete(0, true);
  }

  /**
   * This function overrides the virtual function in the HP::Element class
   * It returns whether or not this mcas class is being referenced by another
   * threaded. It is being referenced if any associated descriptor has a
   * positive reference count or if there is a hazard point watch on it.
   *
   * @return True if watecd
   */
  bool on_is_watched() {
    for (int i = 0; i < row_count_; i++) {
      Helper<T>* helper = cas_rows_[i].helper_.load();
      // The No check flag is true because each was check prior
      // to the call of this descructor.
      if (helper == reinterpret_cast<Helper<T> *>(MCAS_FAIL_CONST)) {
        break;
      } else if (util::memory::rc::is_watched(helper)) {
        return true;
      }
    }
    return false;
  }

 private:
  /**
   * This enum is used to indicate the state of an mcas operation
   * DELETED is used in debugging procedures
   */
  enum class MCasState : std::int8_t {IN_PROGRESS = 0, PASS = 1 , FAIL = 2,
      DELETED = 3};
  /**
   * This function is used to complete a currently executing MCAS operation
   * It is most likely that this operation is in conflict with some other
   * operation and that it was discoved by the dereferencing of an MCH
   * The MCH contains a reference to the row in the MCAS it is associated with
   * and a reference to the final row in the mcas.
   * Using these two, we can complete the remaining rows.
   *
   * @param start_pos the row to begin at
   * @param wfmode if true it ignores the fail count because the operation
   * is in the op_table
   * @return whether or not the mcas succedded.
   */
  bool mcas_complete(int start_pos, bool wfmode = false);

  /**
   * Same as above, but it calculates the start_pos based on the current row
   * then calls the above mcas_complete.
   *
   * @param current_row the last known completed row
   * @return where or not the mcas succedded
   */
  bool mcas_complete(CasRow<T> *current_row);

  /**
   * This function is used to cleanup a completed MCAS operation
   * It removes each MCH placed during this operation, replacing it with the
   * logical value
   */
  void cleanup(bool success);

  /**
   * This function insures that upon its return that *(cas_rows_[pos].address)
   * no longer equals value. Where value is an object that holds an RC bit mark.
   * If the object is not a Helper for this operation, then the standard
   * descriptor remove function is called. This is important to prevent the case
   * where the stack increases beyond reasonable levels when multiple threads
   * are helping to complete the same operation.
   *
   * @param pos the cas_row which is blocked from completing its operaiton
   * @param value the value read at the position
   * @return the new current value
   **/
  T mcas_remove(const int pos, T value);

  // ------
  // Member Data
  // ------

  /* THe array of CAS triples to complete*/
  std::unique_ptr<CasRow<T>[]> cas_rows_;
  /* The state of the mcas operation */
  std::atomic<MCasState> state_ {MCasState::IN_PROGRESS};
  /* The number of cas rows */
  int row_count_ {0};
  /* The max number of rows supported this instance is expecting*/
  int max_rows_;

  friend Helper<T>;
  friend CasRow<T>;
};  // MCAS class

template<class T>
bool MCAS<T>::add_cas_triple(std::atomic<T> *address, T expected_value,
      T new_value) {
  //First checm to make sure the values do not have any reserved bitss
  if (!tervel::util::isValid(reinterpret_cast<void *>(expected_value)) ||
        !tervel::util::isValid(reinterpret_cast<void *>(new_value))) {
    return false;
  } else if (row_count_ == max_rows_) {
    return false;
  } else {
    cas_rows_[row_count_].address_ = address;
    cas_rows_[row_count_].expected_value_ = expected_value;
    cas_rows_[row_count_].new_value_ = new_value;
    cas_rows_[row_count_++].helper_.store(nullptr);

    //Sort them
    // Probally can be done more efficent
    for (int i = (row_count_ - 1); i > 0; i--) {
      if (cas_rows_[i] > cas_rows_[i-1]) {
        swap(cas_rows_[i], cas_rows_[i-1]);
      } else if (cas_rows_[i] == cas_rows_[i-1]) {
        for (; i < row_count_-1; i++) {
          swap(cas_rows_[i], cas_rows_[i+1]);
        }
        row_count_--;

        /* We  remove the row because the address already exists */
        cas_rows_[row_count_].address_ = nullptr;
        cas_rows_[row_count_].expected_value_ = nullptr;
        cas_rows_[row_count_].new_value_ = nullptr;
        return false;
      }
    }
    return true;
  }
}

template<class T>
bool MCAS<T>::execute() {
  tervel::util::ProgressAssurance::check_for_announcement();
  bool res = mcas_complete(0);
  cleanup(res);
  return res;
}

template<class T>
bool MCAS<T>::mcas_complete(CasRow<T> *current_row) {
  int start_pos = 1;
  assert(util::memory::hp::HazardPointer::is_watched(this));
  assert(cas_rows_[0].helper_.load() != nullptr);
  assert(current_row->helper_.load() != nullptr);


  { // TODO(steven): verify this position calculation.
    uintptr_t temp1 = reinterpret_cast<uintptr_t>(&(cas_rows_[0]));
    uintptr_t temp2 = reinterpret_cast<uintptr_t>(current_row);
    start_pos = temp2 - temp1;
    assert(&(cas_rows_[start_pos]) == current_row && "if this hit, I am bad at math");
  }
  return mcas_complete(start_pos, false);
}


// REVIEW(carlos): This function is way too long. If it doesn't fit in ~1 screen
//   of code, I have trouble reading what it's doing. You should break this up
//   into auxiliary functions. You don't need to declare said functions in the
//   .h file, just put them into an unnamed namespace in the .cc file.
// REVIEW(carlos): TBH, the body of this function is impenatrable to me, and I
//   can't review the contents for correctness as-is.
// RESPONSE(steven): I added comments, but I am unsure how to divide it into
// sub functions.
template<class T>
bool MCAS<T>::mcas_complete(int start_pos, bool wfmode) {
  /**
   * Loop for each row in the op, if helping complete another thread's MCAS
   * Start at last known completed row.
   */
  for (int pos = start_pos; pos < row_count_; pos++) {
    util::ProgressAssurance::Limit progAssur;

    CasRow<T> * row = &(cas_rows_[pos]);

    assert(pos == 0 || cas_rows_[pos-1].helper_.load());

    /* Read the current value of the address */
    T current_value = row->address_->load();

    while (row->helper_.load() == nullptr) {
      /* Loop until this row's helper is no longer null */

      if (state_.load() != MCasState::IN_PROGRESS) {
        /* Checks if the operation has been completed */
        return (state_.load() == MCasState::PASS);
      } else if (!wfmode && progAssur.isDelayed()) {
        /* Check if we need to enter wf_mode */
        if (util::RecursiveAction::recursive_depth() == 0) {
          /* If this is our operation then make an annoucnement */
          tervel::util::ProgressAssurance::make_announcement(this);
          assert(state_.load() != MCasState::IN_PROGRESS);
          return (state_.load() == MCasState::PASS);
        } else {
          /* Otherwise perform a recursive return */
          util::RecursiveAction::set_recursive_return();
          return false;
        }
      }

      /* Process the current value at the address */
      /* Now Check if the current value is descriptor */
      if (util::memory::rc::is_descriptor_first(
            reinterpret_cast<void *>(current_value))) {
        /* Remove it by completing the op, try again */
        current_value = this->mcas_remove(pos, current_value);

        /* Check if we are executing a recursive return and if so determine
         * if we are at our own operation or need to return farther. */
        if (util::RecursiveAction::recursive_return()) {
          if (util::RecursiveAction::recursive_depth() == 0) {
            /* we are back to our own operation so re-read process the
             * current value */
            util::RecursiveAction::clear_recursive_return();
            current_value = row->address_->load();
          } else {
            /* we need to return some more */
            return false;
          }
        }
      } else if (current_value != row->expected_value_) {
        /* Current value does not match the expected value and it is a non
         * descriptor type, the mcas operation should fail. */
        Helper<T>* temp_null = nullptr;
        /* First try to disable row by assigning a failed constant */
        if (row->helper_.compare_exchange_strong(temp_null,
              reinterpret_cast<Helper<T> *>(MCAS_FAIL_CONST))
              || temp_null == reinterpret_cast<Helper<T> *>(MCAS_FAIL_CONST)) {
          /* if row was disabled then set the state to FAILED */
          MCasState temp_state = MCasState::IN_PROGRESS;
          this->state_.compare_exchange_strong(temp_state, MCasState::FAIL);
          assert(this->state_.load() == MCasState::FAIL);
          return false;
        } else {
          /* the row was associated, lets re-evaluate the current state */
          continue;
        }
      }  else {
        /* Else the current_value matches the expected_value_ */
        Helper<T>* helper = tervel::util::memory::rc::get_descriptor<
            Helper<T> >(this, row);
        if (row->address_->compare_exchange_strong(current_value,
                reinterpret_cast<T>(util::memory::rc::mark_first(helper)))) {
          /* helper was successfully placed at the address */
          Helper<T> * temp_null = nullptr;
          if (row->helper_.compare_exchange_strong(temp_null, helper)
                || temp_null == helper) {
            /* We successfully associated the helper the row */
            break;  /* break the while loop, on to the next row! */
          } else {
            /* We failed to associate the helper, remove it, this implies that
             * the operation is over
             */
            T temp_helper = reinterpret_cast<T>(
                  util::memory::rc::mark_first(helper));

            row->address_->compare_exchange_strong(temp_helper,
                  row->expected_value_);

            util::memory::rc::free_descriptor(helper);

            if(row->helper_.load() == reinterpret_cast<Helper<T> *>(
                  MCAS_FAIL_CONST)) {
              MCasState temp_state = MCasState::IN_PROGRESS;
              this->state_.compare_exchange_strong(temp_state, MCasState::FAIL);
            }
            assert(row->helper_.load());
            assert(state_.load() != MCasState::IN_PROGRESS);
            return (state_.load() == MCasState::PASS);
          }
        } else {
          /* We failed to place helper, re-evaluate the current_value
           * Set no check to true since it was never used.
           */
          util::memory::rc::free_descriptor(helper, true);
          continue;
        }
      }  // End Else Try to replace
    }  // End While Current helper is null

    if (row->helper_.load() == reinterpret_cast<Helper<T> *>(MCAS_FAIL_CONST)) {
      MCasState temp_state = MCasState::IN_PROGRESS;
      this->state_.compare_exchange_strong(temp_state, MCasState::FAIL);
      assert(this->state_.load() == MCasState::FAIL);
      return false;
    }
  }  // End For Loop on CasRows

  /* All rows have been associated, so set the state to passed! */
  MCasState temp_state = MCasState::IN_PROGRESS;
  if (this->state_.compare_exchange_strong(temp_state, MCasState::PASS)) {
    temp_state = MCasState::PASS;
  }
  assert(this->state_.load() != MCasState::IN_PROGRESS);
  return (temp_state == MCasState::PASS);
}  // End Complete function.

template<class T>
T MCAS<T>::mcas_remove(const int pos, T value) {
  std::atomic<void *> *address = reinterpret_cast<std::atomic<void *>*>(
            cas_rows_[pos].address_);
    util::Descriptor *descr = util::memory::rc::unmark_first(
          reinterpret_cast<void *>(value));

  // First get a watch on the object.
  bool watched = util::memory::rc::watch(descr, address,
          reinterpret_cast<void *>(value));

  if (watched) {
    // Now unwatch it, crazy right? But there is a reason...
    util::memory::rc::unwatch(descr);

    /* If we watched it and it is a MCH for this operation, then act of
     * watching will call the MCH's on_watch function, which will associate it
     * with this cas row...thus if cas_rows_[pos].helper != nullprt, then this
     * could have been a MCH to for this row but it does not matter, because
     * this row is already done, so we need to go to the next row.
     */
    if (this->cas_rows_[pos].helper_.load() != nullptr) {
      return static_cast<T>(nullptr);  // Does not matter, it wont be used.
    } else {
      /* It is some other threads operation, so lets complete it.*/
      util::memory::rc::remove_descriptor(value, address);
    }
  }
  // watch failed do to the value at the address changing, return new value
  return reinterpret_cast<T>(address->load());
}

template<class T>
void MCAS<T>::cleanup(bool success) {
  for (int pos = 0; pos < row_count_; pos++) {
    /* Loop for each row in the op*/
    CasRow<T> * row = &cas_rows_[pos];

    assert(row->helper_.load() != nullptr);

    Helper<T> * temp_helper = row->helper_.load();
    T marked_helper = reinterpret_cast<T>(
          util::memory::rc::mark_first(temp_helper));
    if (temp_helper == reinterpret_cast<Helper<T> *>(MCAS_FAIL_CONST)) {
      // There can not be any any associated rows beyond this position.
      return;
    } else {
      // else there was a helper placed for this row
      T cur_value = row->address_->load();
      if (cur_value == marked_helper) {
        if (success) {
          row->address_->compare_exchange_strong(cur_value, row->new_value_);
        } else {
          row->address_->compare_exchange_strong(cur_value,
                row->expected_value_);
        }
      }  // End If the current value matches the helper placed for this op
    }  // End Else there was a helper placed for this row
  }  // End While loop over casrows.
}  // End cleanup function.

}  // namespace mcas
}  // namespace wf
}  // namespace algorithms
}  // namespace tervel

#endif  // TERVEL_MCAS_MCAS_H_
