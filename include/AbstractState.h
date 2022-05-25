#ifndef ABSSTATE_H
#define ABSSTATE_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <ostream>
#include <sstream>
#include <string>

#include "Address.h"
// Forward declarations

namespace cacheAnaPass {
class AbstractState;

} // namespace cacheAnaPass

class AbstractState {
public: // everything is public, because IDGAF
  std::list<unsigned int> Successors;
  std::list<unsigned int> Predecessors;

  unsigned int Addr;
  unsigned int Unrolled;

  int Computed = 0;
  bool Filled = false;

  // Only entries below this comment are needed for the exercise.

  /**
   * @brief Containing all Abstract Cache Tags.
   *        Key of the list has no Meaning.
   *
   */
  struct Entry {
    std::list<unsigned int> Blocks;
  };

  /**
   * @brief Cache Set, Key is the Age of the Entries.
   *
   */
  struct Set {
    // uInt in this map is the Age.
    std::map<unsigned int, Entry> Associativity;
  };

  /**
   * @brief Cache Sets, key is the Set number [0-15], derived from Address.
   *
   */
  std::map<unsigned int, Set> Sets;

  AbstractState(AbstractState const &Copy) {
    Addr = Copy.Addr;
    Unrolled = Copy.Unrolled;
    for (auto S : Copy.Sets) {
      unsigned int SetNr = S.first;
      for (auto E : S.second.Associativity) {
        unsigned int Age = E.first;
        for (auto B : E.second.Blocks) {
          Sets[SetNr].Associativity[Age].Blocks.push_back(B);
        }
      }
    }
  }

  AbstractState(AbstractState const &Copy, Address Update) {
    Addr = Copy.Addr;
    Unrolled = Copy.Unrolled;
    for (auto S : Copy.Sets) {
      unsigned int SetNr = S.first;
      for (auto E : S.second.Associativity) {
        unsigned int Age = E.first;
        for (auto B : E.second.Blocks) {
          Sets[SetNr].Associativity[Age].Blocks.push_back(B);
        }
      }
    }
    this->update(Update);
  }

  AbstractState() {}

  AbstractState(unsigned int AddressIn) {
    Addr = AddressIn;
    Unrolled = 0;
  }

  AbstractState(unsigned int AddressIn, unsigned int UnrolledIn) {
    Addr = AddressIn;
    Unrolled = UnrolledIn;
  }

  void setUnrolled(unsigned int In) { Unrolled = In; }

  bool operator==(AbstractState In) {
    for (int Index = 0; Index < 16; Index++) {
      for (int Age = 0; Age < 4; Age++) {
        for (auto E1 : Sets[Index].Associativity[Age].Blocks) {
          // find E1 in In States Set and Age.
          if (std::find(In.Sets[Index].Associativity[Age].Blocks.begin(),
                        In.Sets[Index].Associativity[Age].Blocks.end(),
                        E1) == In.Sets[Index].Associativity[Age].Blocks.end()) {
            return false;
          }
        }
        for (auto E2 : In.Sets[Index].Associativity[Age].Blocks) {
          // find E2 in This Set and Age.
          if (std::find(Sets[Index].Associativity[Age].Blocks.begin(),
                        Sets[Index].Associativity[Age].Blocks.end(),
                        E2) == Sets[Index].Associativity[Age].Blocks.end()) {
            return false;
          }
        }
      }
    }
    return true;
  }

  /**
   * @brief Executes an Must LRU Join on the AbstractCacheState
   *
   * @param In, AbstractState that gets joined into the State.
   */
  void mustJoin(AbstractState In) {
    /**
     *  TODO: Fill this function with an LRU must Join.
     * For this you need to use Sets. Associativity and Blocks.
     */

    // Loop through all 16 sets
    for (int Index = 0; Index < 16; Index++) {

      // create a temporary set of associativity
      struct Set temp_set;
      struct Set current_set = Sets[Index];
      struct Set incoming_set = In.Sets[Index];

      // loop through all Ages
      for (auto associativity_map : current_set.Associativity) {

        auto current_associativity_age = associativity_map.first;
        std::list<unsigned int> current_associativity_blocklist =
            associativity_map.second.Blocks;
        // for every element of associativity_block_list
        for (auto block : current_associativity_blocklist) {
          // look through ALL incoming_set.Associativities.Entry.Blocklists for
          // occurrences
          for (auto incoming_associativity : incoming_set.Associativity) {
            auto age = incoming_associativity.first;
            struct Entry entry = incoming_associativity.second;

            auto ret =
                std::find(entry.Blocks.begin(), entry.Blocks.end(), block);
            if (ret != entry.Blocks.end()) {

              // take the maximum age
              auto new_age = std::max(current_associativity_age, age);

              // create a new entry at the maximum age and
              std::list<unsigned int> new_block_list;
              new_block_list.push_back((unsigned int) *ret);
              struct Entry new_entry = {new_block_list};

              // add it to temporary set
              temp_set.Associativity.insert(
                  std::pair<unsigned int, struct Entry>(new_age, new_entry));
            }
          }
        }
        // print_block_list(associativity_age, associativity_block_list);

        Sets[Index] = temp_set;
      }
    }
  }

  /**
   * @brief Checks if Address Addr is in Cache
   *
   * @param Addr Address to check.
   * @return true CacheState contains Address Addr
   * @return false CacheState does not contain Address Addr
   */
  bool isHit(Address Addr) {
    for (auto E : Sets[Addr.Index].Associativity) {
      for (auto B : E.second.Blocks) {
        if (B == Addr.Tag)
          return true;
      }
    }
    return false;
  }

  /**
   * @brief Updates the AbstractState with given Address
   *
   * @param Addr , Address
   */
  void update(Address Addr) {
    // If Updated Address is of Age 0 do nothing
    if (std::find(Sets[Addr.Index].Associativity[0].Blocks.begin(),
                  Sets[Addr.Index].Associativity[0].Blocks.end(),
                  Addr.Tag) != Sets[Addr.Index].Associativity[0].Blocks.end())
      return;
    // This loopages all entries by one. 3 <-2, 2<-1, 1<-0
    for (int I = 3; I > 0; I--) {
      Sets[Addr.Index].Associativity[I] = Sets[Addr.Index].Associativity[I - 1];
      Sets[Addr.Index].Associativity[I].Blocks.remove(Addr.Tag);
    }
    // entry at age 0 is updated with current address.
    Sets[Addr.Index].Associativity[0].Blocks = {Addr.Tag};
  }

  /**
   * @brief Fills the AbstractState PreState and updates with PreAddress.
   *
   * @param PreState, State that fills this state.
   *
   * @param PreAddr Address of PreState
   */
  void fill(AbstractState PreState, Address PreAddr) {
    bool Verbose = false;
    // copy Pre State into this.
    for (auto S : PreState.Sets) {
      unsigned int Index = S.first;
      for (auto E : S.second.Associativity) {
        unsigned int Age = E.first;
        // If updated age is greater 4 The Tag is no longer in Cache.
        // Due to associativity of 4 per set.
        if (Age >= 4)
          break;
        for (auto B : E.second.Blocks) {
          Sets[Index].Associativity[Age].Blocks.push_back(B);
        }
      }
    }
    if (Verbose) {
      std::cout << "Before:\n";
      this->dump();
    }
    // update this with PreAddr
    this->update(PreAddr);
    if (Verbose) {
      std::cout << "Update Tag: " << PreAddr.Tag << "\n";
      std::cout << "Update Set: " << PreAddr.Index << "\n";
      std::cout << "After:\n";
      this->dump();
    }
  }

  void dumpSet(unsigned int Set) {
    std::cout << Addr << " {\n";

    std::cout << "Set[" << Set << "]: \n";
    for (auto EntryPair : this->Sets[Set].Associativity) {
      std::cout << "  Age[" << EntryPair.first << "]: ";
      for (auto Block : EntryPair.second.Blocks) {
        std::cout << Block << " ";
      }
      std::cout << "\n";
    }
    std::cout << "}\n";
  }

  void dump() {
    std::cout << Addr << " {\n";
    std::cout << "Unrolled: " << Unrolled << "\n";
    std::cout << "Computed: " << Computed << "\n";
    std::cout << "Predecessors: ";
    for (auto PreNr : Predecessors) {
      std::cout << PreNr << " ";
    }
    std::cout << "\n";

    std::cout << "Successors: ";
    for (auto SuccNr : Successors) {
      std::cout << SuccNr << " ";
    }
    std::cout << "\n";

    for (auto SetPair : Sets) {
      std::cout << "Set[" << SetPair.first << "]: \n";
      for (auto EntryPair : SetPair.second.Associativity) {
        std::cout << "  Age[" << EntryPair.first << "]: ";
        for (auto Block : EntryPair.second.Blocks) {
          std::cout << Block << " ";
        }
        std::cout << "\n";
      }
    }
    std::cout << "}\n";
  }
};     // namespace
#endif // STATE_H