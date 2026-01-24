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
  testing::RandomSimTest(*LoadGame("hungarian_tarok", {{"players", GameParameter(2)}}), 100);
  testing::RandomSimTest(*LoadGame("hungarian_tarok", {{"players", GameParameter(4)}}), 100);
  auto observer = LoadGame("hungarian_tarok")
                      ->MakeObserver(kDefaultObsType,
                                     GameParametersFromString("single_tensor"));
  testing::RandomSimTestCustomObserver(*LoadGame("hungarian_tarok"), observer);
}

}  // namespace
}  // namespace hungarian_tarok
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarok::BasicHungariantarokTests();
}
