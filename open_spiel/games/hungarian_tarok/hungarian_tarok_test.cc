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

namespace open_spiel {
namespace hungarian_tarok {
namespace {

namespace testing = open_spiel::testing;
HungarianTarokState PostSetupState(std::mt19937 &mt) {
  auto game = LoadGame("hungarian_tarok");
  HungarianTarokState state(
      std::static_pointer_cast<const HungarianTarokGame>(game));
  while (state.GetPhaseType() == PhaseType::kSetup) {
    auto outcomes = state.ChanceOutcomes();
    state.ApplyAction(SampleAction(outcomes, mt).first);
  }
  return state;
}

HungarianTarokState PostBiddingState(std::mt19937 &mt) {
  HungarianTarokState state = PostSetupState(mt);
  while (state.GetPhaseType() == PhaseType::kBidding) {
    auto legal_actions = state.LegalActions();
    std::uniform_int_distribution<> dist(0, legal_actions.size() - 1);
    state.ApplyAction(legal_actions[dist(mt)]);
  }
  return state;
}

HungarianTarokState PostTalonState(std::mt19937 &mt) {
  HungarianTarokState state = PostBiddingState(mt);
  while (state.GetPhaseType() == PhaseType::kTalon) {
    auto outcomes = state.ChanceOutcomes();
    state.ApplyAction(SampleAction(outcomes, mt).first);
  }
  return state;
}

HungarianTarokState PostSkartState(std::mt19937 &mt) {
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
  testing::RandomSimTest(*LoadGame("hungarian_tarok"), 1000);
  // auto observer = LoadGame("hungarian_tarok")
  //                     ->MakeObserver(kDefaultObsType,
  //                                    GameParametersFromString("single_tensor"));
  // testing::RandomSimTestCustomObserver(*LoadGame("hungarian_tarok"),
  // observer);
}

void ConsolePlayHungariantarokTest() {
  std::mt19937 mt(1234);
  HungarianTarokState state = PostSkartState(mt);
  testing::ConsolePlayTest(*LoadGame("hungarian_tarok"), &state);
}

} // namespace
} // namespace hungarian_tarok
} // namespace open_spiel

int main(int argc, char **argv) {
  open_spiel::hungarian_tarok::BasicHungariantarokTests();
  open_spiel::hungarian_tarok::ConsolePlayHungariantarokTest();
}
