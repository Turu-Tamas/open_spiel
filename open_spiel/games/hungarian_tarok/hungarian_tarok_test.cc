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

namespace open_spiel {
namespace hungarian_tarok {
namespace {

namespace testing = open_spiel::testing;

void BasicHungariantarokTests() {
  testing::LoadGameTest("hungarian_tarok");
  testing::ChanceOutcomesTest(*LoadGame("hungarian_tarok"));
  testing::RandomSimTest(*LoadGame("hungarian_tarok"), 100);
  testing::RandomSimTest(*LoadGame("hungarian_tarok",
                         {{"action_mapping", GameParameter(true)}}), 100);
  testing::RandomSimTest(*LoadGame("hungarian_tarok",
                         {{"suit_isomorphism", GameParameter(true)}}), 100);
  for (Player players = 3; players <= 5; players++) {
    testing::RandomSimTest(
        *LoadGame("hungarian_tarok", {{"players", GameParameter(players)}}), 100);
  }
  testing::ResampleInfostateTest(*LoadGame("hungarian_tarok"), /*num_sims=*/100);
  auto observer = LoadGame("hungarian_tarok")
                      ->MakeObserver(kDefaultObsType,
                                     GameParametersFromString("single_tensor"));
  testing::RandomSimTestCustomObserver(*LoadGame("hungarian_tarok"), observer);
}

void PolicyTest() {
  using PolicyGenerator = std::function<TabularPolicy(const Game& game)>;
  std::vector<PolicyGenerator> policy_generators = {
      GetAlwaysFoldPolicy,
      GetAlwaysCallPolicy,
      GetAlwaysRaisePolicy
  };

  std::shared_ptr<const Game> game = LoadGame("hungarian_tarok");
  for (const auto& policy_generator : policy_generators) {
    testing::TestEveryInfostateInPolicy(policy_generator, *game);
    testing::TestPoliciesCanPlay(policy_generator, *game);
  }
}

void StartingPlayerTest() {
  std::shared_ptr<const Game> game =
      LoadGame("hungarian_tarok", {{"players", GameParameter(3)},
                               {"starting_player", GameParameter(1)}});
  std::unique_ptr<State> state = game->NewInitialState();
  SPIEL_CHECK_TRUE(state->IsChanceNode());
  // In 3-player Leduc, deck is J,Q,K,A of 2 suits.
  // J=0,1; Q=2,3; K=4,5; A=6,7
  state->ApplyAction(0);  // P0 gets J
  state->ApplyAction(2);  // P1 gets Q
  state->ApplyAction(4);  // P2 gets K

  // Round 1 betting. Starting player is 1.
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 1);
  state->ApplyAction(ActionType::kFold);   // P1 folds.
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 2);
  state->ApplyAction(ActionType::kRaise);  // P2 raises.
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 0);
  state->ApplyAction(ActionType::kCall);   // P0 calls.

  // Round 2, deal public card.
  SPIEL_CHECK_TRUE(state->IsChanceNode());
  state->ApplyAction(3);  // Public card is a Q.
  // Player 1 folded, so player 2 should be next.
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 2);
}

}  // namespace
}  // namespace hungarian_tarok
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarok::BasicHungariantarokTests();
  open_spiel::hungarian_tarok::PolicyTest();
  open_spiel::hungarian_tarok::StartingPlayerTest();
}
