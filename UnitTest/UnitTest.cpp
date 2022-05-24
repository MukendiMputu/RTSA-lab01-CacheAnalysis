#include <gtest/gtest.h>
#include <sys/stat.h>

#include "../include/AbstractCache.h"

void fillSet(AbstractState &In, unsigned int Set) {
  for (int I = 0; I < 4; I++) {
    In.Sets[Set].Associativity[I].Blocks.push_front(I);
  }
}

// Joined Set should remain the same
TEST(MustJoinTests, MustJoinSameStates) {
  AbstractCache AC;
  AC.addEmptyNode(0);
  fillSet(AC.Nodes[0], 0);
  AC.addEmptyNode(1);
  fillSet(AC.Nodes[1], 0);

  AC.Nodes[0].mustJoin(AC.Nodes[1]);
  for (auto B : AC.Nodes[0].Sets[0].Associativity) {
    uint Age = B.first;
    EXPECT_EQ(AC.Nodes[0].Sets[0].Associativity[Age].Blocks.front(),
              AC.Nodes[1].Sets[0].Associativity[Age].Blocks.front());
  }
}

void counterFillSet(AbstractState &In, unsigned int Set) {
  for (int I = 0; I < 4; I++) {
    In.Sets[Set].Associativity[I].Blocks.push_front((I + 1) % 4);
  }
}

// Resulting state should be
// Set[0]:
//   Age[0]:
//   Age[1]: 1
//   Age[2]: 2
//   Age[3]: 0 3
TEST(MustJoinTests, MustJoinDifferentStates) {
  AbstractCache AC;
  AC.addEmptyNode(0);
  counterFillSet(AC.Nodes[0], 0);
  AC.addEmptyNode(1);
  fillSet(AC.Nodes[1], 0);
  std::cout << "==Before:==\n";
  AC.Nodes[0].dumpSet(0);
  AC.Nodes[1].dumpSet(0);

  AC.Nodes[0].mustJoin(AC.Nodes[1]);
  std::cout << "\n==After:==\n";
  AC.Nodes[0].dumpSet(0);
  for (auto B : AC.Nodes[0].Sets[0].Associativity) {
    uint Age = B.first;
    switch (Age) {
    case 0:
      EXPECT_TRUE(AC.Nodes[0].Sets[0].Associativity[Age].Blocks.empty());
      break;
    case 1:
      EXPECT_EQ(AC.Nodes[0].Sets[0].Associativity[Age].Blocks.front(), 1);
      break;
    case 2:
      EXPECT_EQ(AC.Nodes[0].Sets[0].Associativity[Age].Blocks.front(), 2);
      break;
    case 3:
      // this test is not very exact the Set 0 with age 3 should contain (0,3)
      // in arbitrary order
      EXPECT_EQ(AC.Nodes[0].Sets[0].Associativity[Age].Blocks.size(), 2);
      break;
    default:
      // should never be reached!
      EXPECT_TRUE(false);
    }
  }
}

void fillSetHighNumbers(AbstractState &In, unsigned int Set) {
  for (int I = 0; I < 4; I++) {
    In.Sets[Set].Associativity[I].Blocks.push_front(10 + I);
  }
}

// resulting state should be empty
TEST(MustJoinTests, MustJoinDisjunctStates) {
  AbstractCache AC;
  AC.addEmptyNode(0);
  fillSetHighNumbers(AC.Nodes[0], 0);
  AC.addEmptyNode(1);
  fillSet(AC.Nodes[1], 0);
  std::cout << "==Before:==\n";
  AC.Nodes[0].dumpSet(0);
  AC.Nodes[1].dumpSet(0);

  AC.Nodes[0].mustJoin(AC.Nodes[1]);
  std::cout << "\n==After:==\n";
  AC.Nodes[0].dumpSet(0);
  for (auto B : AC.Nodes[0].Sets[0].Associativity) {
    uint Age = B.first;
    EXPECT_TRUE(AC.Nodes[0].Sets[0].Associativity[Age].Blocks.empty());
  }
}

TEST(MustJoinTests, MustJoinDisjunctStatesAllSets) {
  AbstractCache AC;
  AC.addEmptyNode(0);
  AC.addEmptyNode(1);
  for (int I = 0; I < 16; I++) {
    fillSetHighNumbers(AC.Nodes[0], I);
    fillSet(AC.Nodes[1], I);
  }

  std::cout << "==Before:==\n";
  AC.Nodes[0].dump();
  AC.Nodes[1].dump();

  AC.Nodes[0].mustJoin(AC.Nodes[1]);
  std::cout << "\n==After:==\n";
  AC.Nodes[0].dump();
  for (auto Set : AC.Nodes[0].Sets) {
    for (auto B : Set.second.Associativity) {
      EXPECT_TRUE(B.second.Blocks.empty());
    }
  }
}

TEST(MustJoinTests, MustJoinDifferentStatesAllSets) {
  AbstractCache AC;
  AC.addEmptyNode(0);
  AC.addEmptyNode(1);
  for (int I = 0; I < 16; I++) {
    if (I % 2)
      counterFillSet(AC.Nodes[0], I);
    fillSet(AC.Nodes[1], I);
  }
  std::cout << "==Before:==\n";
  AC.Nodes[0].dump();
  AC.Nodes[1].dump();

  AC.Nodes[0].mustJoin(AC.Nodes[1]);

  AC.Nodes[1].mustJoin(AC.Nodes[0]);
  std::cout << "\n==After:==\n";
  AC.Nodes[1].dump();
  for (auto Set : AC.Nodes[1].Sets) {
    for (auto B : Set.second.Associativity) {
      uint SetNr = Set.first;
      uint Age = B.first;
      if (SetNr % 2) {
        switch (Age) {
        case 0:
          EXPECT_TRUE(B.second.Blocks.empty());
          break;
        case 1:
          EXPECT_EQ(B.second.Blocks.front(), 1);
          break;
        case 2:
          EXPECT_EQ(B.second.Blocks.front(), 2);
          break;
        case 3:
          // this test is not very exact the Set 0 with age 3 should contain
          // (0,3) in arbitrary order
          EXPECT_EQ(B.second.Blocks.size(), 2);
          break;
        default:
          // should never be reached!
          EXPECT_TRUE(false);
        }
      } else {
        EXPECT_TRUE(B.second.Blocks.empty());
      }
    }
  }
  EXPECT_TRUE(AC.Nodes[1] == AC.Nodes[0]);
}