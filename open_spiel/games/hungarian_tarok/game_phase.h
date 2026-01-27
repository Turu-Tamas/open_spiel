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

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_GAME_PHASE_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_GAME_PHASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/spiel.h"

namespace open_spiel {
namespace hungarian_tarok {

class HungariantarokState;

// Abstract interface for a single phase of Hungarian Tarok.
//
// Intended usage pattern:
// - `HungariantarokState` owns the current phase (e.g. via
//   `std::unique_ptr<GamePhase> phase_`).
// - `HungariantarokState` implements the OpenSpiel `State` virtuals by
//   delegating to `*phase_`.
// - When a phase finishes, the state can replace `phase_` with the successor.
class GamePhase {
 public:
	virtual ~GamePhase() = default;

	// Delegation points corresponding to the OpenSpiel `State` API.
	virtual Player CurrentPlayer() const = 0;
	virtual std::vector<Action> LegalActions() const = 0;

	virtual void DoApplyAction(Action action) = 0;

	virtual bool GameOver() const;
	virtual bool PhaseOver() const = 0;
	// virtual std::string ActionToString(Player player, Action action) const = 0;
	// virtual std::string ToString() const = 0;

	// virtual std::string ObservationString(Player player) const = 0;
	// virtual void ObservationTensor(Player player, absl::Span<float> values) const = 0;
};

}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_GAME_PHASE_H_
