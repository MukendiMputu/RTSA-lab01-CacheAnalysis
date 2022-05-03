
#ifndef ABSTRACHTCACHESTATE_H
#define ABSTRACHTCACHESTATE_H

#include <cassert>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <ostream>
#include <utility>

#include <llvm/IR/BasicBlock.h>
#include <llvm/Support/raw_ostream.h>

#include "AbstractState.h"
#include "Address.h"
#include "ConcreteState.h"

// Forward declarations

namespace cacheAnaPass {
class AbstractCache;

} // namespace cacheAnaPass

class AbstractCache {
public: // everything is public, because IDGAF
  // map keys are instruction Addresses.
  std::map<unsigned int, std::list<unsigned int>> Edges;
  std::map<unsigned int, AbstractState> Nodes;

  AbstractCache() {}

  /**
   * @brief Add an Edge to the Abstract Cache
   *
   * @param Pre Predecessor Address
   * @param Suc Successor Address
   */
  void addEdge(unsigned int Pre, unsigned int Suc) {
    Edges[Pre].push_back(Suc);
    Nodes[Pre].Successors.push_back(Suc);
    Nodes[Suc].Predecessors.push_back(Pre);
  }

  /**
   * @brief Add an Edge to the AbstractStateGraph
   *
   * @param Pre
   * @param Suc
   */
  void removeEdge(unsigned int Pre, unsigned int Suc) {
    Edges[Pre].remove(Suc);
    Nodes[Pre].Successors.remove(Suc);
    Nodes[Suc].Predecessors.remove(Pre);
  }

  /**
   * @brief Add an Empty node @NodeAddr
   *
   * @param NodeAddr
   * @return unsigned int
   */
  unsigned int addEmptyNode(unsigned int NodeAddr) {
    int I = Nodes.size();
    Nodes[I] = AbstractState(NodeAddr);
    return I;
  }

  /**
   * @brief Returns True if a path From -> To exists.
   *
   * @param From
   * @param To
   * @return true
   * @return false
   */
  bool findPath(unsigned int From, unsigned int To) {
    std::map<unsigned int, bool> Visited;
    Visited[From] = false;
    bool Ret = false;
    for (auto Visitor : Visited) {
      if (!Visitor.second) {
        for (unsigned int Next : Edges[Visitor.first]) {
          if (Next == To) {
            return true;
          }
          Visited[Next] = false;
        }
      }
      Visited[Visitor.first] = true;
    }
    return false;
  }

  /**
   * @brief  Removes all Nested loops from the handed LoopBody
   *
   * @param LoopBodyIn
   * @param OrigNodeToUnrolledNode
   */
  void removeNestedLoops(
      std::list<unsigned int> LoopBodyIn,
      std::map<unsigned int, unsigned int> OrigNodeToUnrolledNode) {
    unsigned int LoopHead = LoopBodyIn.front();
    unsigned int LoopTail = LoopBodyIn.back();
    unsigned int NestLoopTail;
    for (unsigned int NodeNr : LoopBodyIn) {
      bool IsLoopHead = false;
      bool FoundLoopBody = false;
      unsigned int LoopBodySize = 0;
      int NestLoopHead = 0;
      NestLoopHead = NodeNr;
      if (Nodes[NodeNr].Predecessors.size() > 1) {
        IsLoopHead = true;
        FoundLoopBody = false;
        LoopBodySize++;
        // is loop head?
        for (unsigned int Pre : Nodes[NodeNr].Predecessors) {
          if (Pre > NodeNr) {
            // Might be loop head.
            // check if all States between Pre and NodeNr are a coherent set.
            for (unsigned int I = NodeNr; I < Pre; I++) {
              // Check if all out going edges are in the set
              for (unsigned int Succ : Nodes[I].Successors) {
                if (Succ > Pre) {
                  //  Set is not coherent
                  IsLoopHead = false;
                  break;
                }
              }
              // check if all incoming edges are in the set.
              if (IsLoopHead && I != NodeNr)
                for (unsigned int Pred : Nodes[I].Predecessors) {
                  if (Pred < NodeNr) {
                    //  Set is not coherent
                    IsLoopHead = false;
                    break;
                  }
                }
              FoundLoopBody = true;
              LoopBodySize++;
            }
            NestLoopTail = Pre;
          } else if (!FoundLoopBody) {
            // If no coherent Loopbody exist we cannot unroll.
            NestLoopHead = 0;
            IsLoopHead = false;
          }
          if (FoundLoopBody) {
            // Check if a Path between Head and Tail exists,
            // if not its not a loop.
            if (findPath(NestLoopHead, NestLoopTail))
              removeEdge(OrigNodeToUnrolledNode[NestLoopTail],
                         OrigNodeToUnrolledNode[NestLoopHead]);
          }
        }
      }
    }
  }

  /**
   * @brief Unroll Loops.
   *
   * @param NodeNr
   */
  void unrollLoops() {
    unsigned int NestedBorder = 0;
    unsigned int LastNode = Nodes.size();
    unsigned int IterationCounter = 0;
    for (std::pair<const unsigned int, AbstractState> NodePair : Nodes) {
      IterationCounter++;
      if (NodePair.first == LastNode) {
        break;
      }
      unsigned int NodeNr = NodePair.first;
      // Don't unroll nested loops
      if (NodeNr < NestedBorder)
        continue;
      bool IsLoopHead = false;
      bool FoundLoopBody = false;
      bool Verbose = false;
      std::list<unsigned int> LoopBody;
      std::list<unsigned int> AdditionalLoopTails;
      if (Nodes[NodeNr].Predecessors.size() > 1) {
        IsLoopHead = true;
        // is loop head?
        for (unsigned int Pre : Nodes[NodeNr].Predecessors) {
          if (Pre > NodeNr) {
            // Might be loop head.
            // check if all States between Pre and NodeNr are a coherent set.
            for (unsigned int I = NodeNr; I < Pre; I++) {
              // Check if all out going edges are in the set
              for (unsigned int Succ : Nodes[I].Successors) {
                for (unsigned int PreI : Nodes[I].Predecessors) {
                  // Handle if we have multiple Loopheads.
                  if (PreI >= Pre && I != NodeNr) {
                    // I and Pre are Looptail.
                    {
                      if (std::find(AdditionalLoopTails.begin(),
                                    AdditionalLoopTails.end(),
                                    I) == AdditionalLoopTails.end()) {
                        AdditionalLoopTails.push_back(I);
                        break;
                      }
                    }
                  }
                  if (std::find(LoopBody.begin(), LoopBody.end(), I) ==
                      LoopBody.end())
                    LoopBody.push_back(I);

                  if (Succ > Pre) {
                    //  Set is not coherent
                    IsLoopHead = false;
                    break;
                  }
                }
              }
              // check if all incoming edges are in the set.
              if (IsLoopHead && I != NodeNr)
                for (unsigned int Pred : Nodes[I].Predecessors) {
                  if (Pred < NodeNr) {
                    //  Set is not coherent
                    IsLoopHead = false;
                    break;
                  }
                }
              FoundLoopBody = true;
            }
            LoopBody.push_back(Pre);
          } else if (!FoundLoopBody) {
            // If no coherent Loopbody exist we cannot unroll.
            LoopBody.clear();
            IsLoopHead = false;
          }
        }
      }
      // Found Loop Head and Body!
      // Add empty unrolled Nodes
      // Map points from OrigNode To Unrolled Node.
      if (FoundLoopBody) {
        std::map<unsigned int, unsigned int> OrigNodeToUnrolledNode;
        for (unsigned int Node : LoopBody) {
          // Node to unroll
          AbstractState UnrolledNode(Nodes[Node]);
          UnrolledNode.setUnrolled(1);
          unsigned int I = Nodes.size();
          Nodes[I] = UnrolledNode;
          OrigNodeToUnrolledNode[Node] = I;
          assert(Nodes[OrigNodeToUnrolledNode[Node]].Unrolled == 1);
          assert(Nodes[Node].Addr == Nodes[OrigNodeToUnrolledNode[Node]].Addr);
        }

        // LoopTail and Head have to be processed different
        unsigned int LoopTail = LoopBody.back();
        LoopBody.pop_back();
        NestedBorder = LoopTail;
        unsigned int LoopHead = LoopBody.front();
        LoopBody.pop_front();

        // Find State entering to LoopHead ()
        unsigned int LoopHeadEntry = 0;
        for (unsigned int Pre : Nodes[LoopHead].Predecessors) {
          if (Pre < LoopHead) {
            LoopHeadEntry = Pre;
            break;
          }
        }

        // Make LoopHeadEntry point to unrolled state instead of the loop.
        addEdge(LoopHeadEntry, OrigNodeToUnrolledNode[LoopHead]);
        removeEdge(LoopHeadEntry, LoopHead);
        // Connect unrolled Loop to the the original Loop.
        if (AdditionalLoopTails.size() == 0)
          addEdge(OrigNodeToUnrolledNode[LoopTail], LoopHead);
        for (auto Tail : AdditionalLoopTails)
          addEdge(OrigNodeToUnrolledNode[Tail], LoopHead);

        // Fix all other states
        addEdge(OrigNodeToUnrolledNode[LoopBody.back()],
                OrigNodeToUnrolledNode[LoopTail]);
        for (unsigned int Node : LoopBody) {
          for (unsigned int Pre : Nodes[Node].Predecessors) {
            // if (std::find(LoopBody.begin(), LoopBody.end(), Pre) !=
            //     LoopBody.end())
            // Add All predecessors and successors to unrolled Nodes
            addEdge(OrigNodeToUnrolledNode[Pre], OrigNodeToUnrolledNode[Node]);
          }
        }

        // Remove Nested loops in unrolled loop
        removeNestedLoops(LoopBody, OrigNodeToUnrolledNode);

        if (Verbose && FoundLoopBody) {
          llvm::outs() << "Found LoopHead @: " << NodeNr << "\n";
          llvm::outs() << "With LoopTail @: " << LoopTail << "\n";
          llvm::outs() << "With Body: {\n";
          int I = 1;
          for (auto Node : LoopBody) {
            llvm::outs() << Node << ", ";
            if (!(I++ % 5)) {
              llvm::outs() << "\n";
            }
          }
          llvm::outs() << "}\n";
          llvm::outs() << "Unrolled States: {\n";
          I = 1;
          for (auto Node : LoopBody) {
            llvm::outs() << OrigNodeToUnrolledNode[Node] << ", ";
            if (!(I++ % 5)) {
              llvm::outs() << "\n";
            }
          }
          llvm::outs() << "}\n";
          I = 1;
          llvm::outs() << "OrigNodeToUnrolledNode: {\n";
          for (auto Nr : OrigNodeToUnrolledNode) {
            llvm::outs() << Nr.first << "->" << Nr.second << ", ";
            if (!(I++ % 3))
              llvm::outs() << "\n";
          }
          llvm::outs() << "}\n";
        }
      }
    }
    return;
  }

  /**
   * @brief Idea fill the graph with the node and perform loop unrolling.
   *
   * @param NodeNr
   */
  void fillAbstractCache(unsigned int NodeNr) {
    // if(isLoopHead(NodeNr))
    Nodes[NodeNr].Computed = true;
    for (unsigned int SuccNr : Nodes[NodeNr].Successors) {
      Nodes[SuccNr];
      // first Run
      if (Nodes[SuccNr].Computed) {
        // Join don't call
        AbstractState Before(Nodes[SuccNr]);
        Nodes[SuccNr].mustJoin(AbstractState(NodeNr, Address(NodeNr)));

        // Continue Joining until State converges
        if (!(Before == Nodes[SuccNr])) {
          fillAbstractCache(NodeNr);
        }
      } else {
        // Update and fill Succ
        Nodes[SuccNr].fill(Nodes[NodeNr], NodeNr);
        fillAbstractCache(SuccNr);
      }
    }
    return;
  }

  /**
   * @brief Return number of measured Hits
   *
   * @return unsigned int
   */
  unsigned int collectHits() {
    unsigned int Hits = 0;
    for (auto const &E : Edges) {
      auto Predecessor = Nodes[E.first];
      for (unsigned int SuccessorAddr : E.second) {
        // When successors Address is in predecessor, we have a Hit.
        Hits += Predecessor.isHit(Address(SuccessorAddr)) ? 1 : 0;
      }
    }
    return Hits;
  }

  /**
   * @brief Return number of measured Misses
   *
   * @return unsigned int
   */
  unsigned int collectMisses() {
    unsigned int Misses = 0;
    for (auto const &E : Edges) {
      auto Predecessor = Nodes[E.first];
      for (unsigned int SuccessorAddr : E.second) {
        // When successors Address is in predecessor, we have a Hit.
        Misses += Predecessor.isHit(Address(SuccessorAddr)) ? 0 : 1;
      }
    }
    return Misses;
  }

  /**
   * @brief Prints all Edges to Console
   *
   */
  void dumpEdges() {
    llvm::outs() << "Dumping Edges:\n";
    for (auto const &E : Edges) {
      llvm::outs() << E.first;
      bool FirstPrint = true;
      for (unsigned int To : E.second) {
        if (FirstPrint) {
          llvm::outs() << " -> " << To;
          FirstPrint = false;
        } else {
          llvm::outs() << ", " << To;
        }
      }
      llvm::outs() << "\n";
    }
  }

  /**
   * @brief Dumps the Graph to a out.dot file
   *
   */
  void dumpDotFile() {
    bool PrintOld = true;
    std::ofstream DotFile;
    DotFile.open("out.dot");
    DotFile << "digraph g {"
            << "\n";
    for (auto const &E : Edges) {
      for (unsigned int To : E.second) {
        if (PrintOld) {
          DotFile << E.first << " -> " << To << "\n";
          if (Nodes[E.first].Unrolled) {
            DotFile << E.first << " [color = red]\n";
          }
        } else {
          DotFile << Nodes[E.first].Addr << "." << Nodes[E.first].Unrolled
                  << " -> " << Nodes[To].Addr << "." << Nodes[To].Unrolled
                  << "\n";
        }
      }
    }
    DotFile << "}\n";
    DotFile.close();
  }

  /**
   * @brief Prints all nodes to Console
   *
   */
  void dumpNodes() {
    for (auto const &E : Edges) {
      Nodes[E.first].dump();
    }
  }
};     // namespace
#endif // ABSTRACHTCACHESTATE_H