// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

#include <functional>
#include <memory>
#include <vector>

#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/policy.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/tests/basic_tests.h"
#include "open_spiel/tests/console_play_test.h"
#include "phases.h"

namespace open_spiel {
namespace hungarian_tarok {
namespace {

namespace testing = open_spiel::testing;
HungarianTarokState PostBiddingState(std::mt19937& mt) {
  HungarianTarokState state = DealHelper().PostSetup();
  while (!state.IsTerminal() && state.GetPhaseType() == PhaseType::kBidding) {
    auto legal_actions = state.LegalActions();
    std::uniform_int_distribution<> dist(0, legal_actions.size() - 1);
    state.ApplyAction(legal_actions[dist(mt)]);
  }
  return state;
}

HungarianTarokState PostTalonState(std::mt19937& mt) {
  HungarianTarokState state = PostBiddingState(mt);
  while (!state.IsTerminal() && state.GetPhaseType() == PhaseType::kTalon) {
    SPIEL_CHECK_EQ(state.CurrentPlayer(), kChancePlayerId);
    auto outcomes = state.ChanceOutcomes();
    state.ApplyAction(SampleAction(outcomes, mt).first);
  }
  return state;
}

HungarianTarokState PostSkartState(std::mt19937& mt) {
  HungarianTarokState state = PostTalonState(mt);
  while (state.GetPhaseType() == PhaseType::kSkart) {
    auto legal_actions = state.LegalActions();
    std::uniform_int_distribution<> dist(0, legal_actions.size() - 1);
    state.ApplyAction(legal_actions[dist(mt)]);
  }
  return state;
}

void BasicHungariantarokTests() {
  testing::LoadGameTest("hungarian_tarok");
  testing::ChanceOutcomesTest(*LoadGame("hungarian_tarok"));
  testing::RandomSimTest(*LoadGame("hungarian_tarok"), 500);
  // auto observer = LoadGame("hungarian_tarok")
  //                     ->MakeObserver(kDefaultObsType,
  //                                    GameParametersFromString("single_tensor"));
  // testing::RandomSimTestCustomObserver(*LoadGame("hungarian_tarok"),
  // observer);
}

void TrialThreeTest(std::mt19937& mt) {
  for (int i = 0; i < 200; i++) {
    HungarianTarokState state = DealHelper().PostSetup();
    bool has_honour = state.PlayerHoldsCard(3, kPagat) ||
                      state.PlayerHoldsCard(3, kSkiz) ||
                      state.PlayerHoldsCard(3, kXXI);
    if (has_honour) continue;

    while (state.GetPhaseType() == PhaseType::kBidding) {
      std::vector<Action> legal_actions = state.LegalActions();
      if (state.CurrentPlayer() == 3) {
        SPIEL_CHECK_TRUE(
            absl::c_find(legal_actions, Bid{3, false}.ToAction()) !=
            legal_actions.end());

        state.ApplyAction(Bid{3, false}.ToAction());
      } else {
        state.ApplyAction(Bid::PassAction());
      }
    }

    int count = 0;
    while (!state.IsTerminal() && state.GetPhaseType() == PhaseType::kTalon) {
      auto outcomes = state.ChanceOutcomes();
      Action action = SampleAction(outcomes, mt).first;
      state.ApplyAction(action);
      count++;
    }

    bool drew_honour = state.PlayerHoldsCard(3, kPagat) ||
                       state.PlayerHoldsCard(3, kSkiz) ||
                       state.PlayerHoldsCard(3, kXXI);
    if (drew_honour) {
      SPIEL_CHECK_FALSE(state.IsTerminal());
    } else {
      SPIEL_CHECK_TRUE(state.IsTerminal());
    }
  }
}

HungarianTarokState MakeState(std::mt19937& mt, Player pagat, Player xxi,
                              Player skiz, Player XVIII, Player XIX,
                              Player XX) {
  DealHelper deal_helper;
  deal_helper.SetCardDestination(kPagat, pagat);
  deal_helper.SetCardDestination(kSkiz, skiz);
  deal_helper.SetCardDestination(kXXI, xxi);
  deal_helper.SetCardDestination(MakeTarok(18), XVIII);
  deal_helper.SetCardDestination(MakeTarok(19), XIX);
  deal_helper.SetCardDestination(MakeTarok(20), XX);
  HungarianTarokState state = deal_helper.PostSetup();
  return state;
}

HungarianTarokState TestBidSequence(
    HungarianTarokState state, std::mt19937& mt,
    std::vector<std::optional<Bid>> bids,
    std::optional<Card> called_card = std::nullopt) {
  for (std::optional<Bid> bid : bids) {
    if (!bid.has_value()) {
      state.ApplyAction(Bid::PassAction());
      continue;
    }

    SPIEL_CHECK_EQ(state.GetPhaseType(), PhaseType::kBidding);
    std::vector<Action> legal_actions = state.LegalActions();
    SPIEL_CHECK_TRUE(absl::c_find(legal_actions, bid->ToAction()) !=
                     legal_actions.end());
    state.ApplyAction(bid->ToAction());
  }
  SPIEL_CHECK_EQ(state.GetPhaseType(), PhaseType::kTalon);
  SPIEL_CHECK_EQ(state.MandatoryCalledCard(), called_card);
  return state;
}

void ConsolePlayHungariantarokTest() {
  std::mt19937 mt(1234);
  HungarianTarokState state = PostSkartState(mt);
  testing::ConsolePlayTest(*LoadGame("hungarian_tarok"), &state);
}

void TestBids(std::mt19937& mt) {
  // straight XIX invit, accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/3, /*skiz=*/2,
                            /*XVIII=*/0, /*XIX=*/2, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      std::nullopt,
                      Bid{2, false},
                      Bid{1, false},
                  },
                  /*called_card=*/MakeTarok(19));
  // straight XVIII invit, accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/0, /*skiz=*/2,
                            /*XVIII=*/0, /*XIX=*/2, /*XX=*/2),
                  mt,
                  {
                      Bid{1, false},
                      std::nullopt,
                      Bid{0, false},
                      std::nullopt,
                  },
                  /*called_card=*/MakeTarok(18));
  // straight solo bid
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/1,
                            /*XVIII=*/1, /*XIX=*/2, /*XX=*/2),
                  mt,
                  {
                      Bid{0, false},
                      std::nullopt,
                      std::nullopt,
                      std::nullopt,
                  },
                  /*called_card=*/std::nullopt);
  // straight XIX invit, not accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/1,
                            /*XVIII=*/1, /*XIX=*/0, /*XX=*/2),
                  mt,
                  {
                      Bid{2, false},
                      std::nullopt,
                      std::nullopt,
                      std::nullopt,
                  },
                  /*called_card=*/std::nullopt);
  // XIX invit, accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/2,
                            /*XVIII=*/1, /*XIX=*/1, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      Bid{3, false},
                      Bid{2, false},
                      std::nullopt,

                      Bid{1, false},  // P1
                      Bid{1, true},   // P2
                  },
                  /*called_card=*/MakeTarok(19));
  // XIX invit, not accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/2,
                            /*XVIII=*/1, /*XIX=*/1, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      Bid{3, false},
                      Bid{2, false},
                      std::nullopt,

                      Bid{1, false},  // P1
                      std::nullopt,   // P2
                  },
                  /*called_card=*/std::nullopt);
  // yielded game, XX called
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/2, /*skiz=*/3,
                            /*XVIII=*/1, /*XIX=*/1, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      std::nullopt,
                      Bid{3, false},
                      Bid{2, false},

                      std::nullopt,  // P2
                  },
                  /*called_card=*/MakeTarok(20));
}
}  // namespace
}  // namespace hungarian_tarok
}  // namespace open_spiel

int main(int argc, char** argv) {
  using namespace open_spiel::hungarian_tarok;
  std::mt19937 mt(42);
  std::shared_ptr<const open_spiel::Game> game =
      open_spiel::LoadGame("hungarian_tarok");

  TestBids(mt);
  TrialThreeTest(mt);
  BasicHungariantarokTests();
  // ConsolePlayHungariantarokTest();
}
