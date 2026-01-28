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

// Placeholder implementation for the "hungarian_tarok" game.
//
// This file previously contained Leduc-poker-derived code. The poker-specific
// concepts (betting, pot, raises, private/public poker cards, showdown, etc.)
// are intentionally removed.
//
// No Hungarian Tarok rules are implemented yet.

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel.h"

#include "game_phase.h"

namespace open_spiel {
namespace hungarian_tarok {

// Minimal placeholder action set.
enum ActionType : Action { kPass = 0 };

class HungarianTarokGame;

class HungarianTarokState : public State {
 public:
  explicit HungarianTarokState(std::shared_ptr<const Game> game);
  HungarianTarokState(const HungarianTarokState& other);

  Player CurrentPlayer() const override;
  std::string ActionToString(Player player, Action move) const override;
  std::string ToString() const override;
  bool IsTerminal() const override;
  std::vector<double> Returns() const override;
  std::vector<Action> LegalActions() const override;
  std::vector<std::pair<Action, double>> ChanceOutcomes() const override;

  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;

  std::unique_ptr<State> Clone() const override;

 protected:
  void DoApplyAction(Action move) override;

 private:
  std::unique_ptr<GamePhase> phase_;
};

class HungarianTarokGame : public Game {
 public:
  explicit HungarianTarokGame(const GameParameters& params);

  int NumDistinctActions() const override { return 1; }
  std::unique_ptr<State> NewInitialState() const override;
  int MaxChanceOutcomes() const override { return 1; }
  int NumPlayers() const override;
  double MinUtility() const override { return -1000; }
  double MaxUtility() const override { return +1000; }
  absl::optional<double> UtilitySum() const override { return 0; }
  std::vector<int> ObservationTensorShape() const override;
  int MaxGameLength() const override;

  std::string ActionToString(Player player, Action action) const override;

  std::shared_ptr<Observer> MakeObserver(
      absl::optional<IIGObservationType> iig_obs_type,
      const GameParameters& params) const override;

 private:
};

}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_H_
