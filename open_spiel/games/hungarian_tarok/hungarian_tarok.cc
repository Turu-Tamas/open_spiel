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

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/memory/memory.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_globals.h"
#include "open_spiel/spiel_utils.h"
#include "phases.h"

namespace open_spiel {
namespace hungarian_tarok {
namespace {

const GameType kGameType{/*short_name=*/"hungarian_tarok",
                         /*long_name=*/"Hungarian Tarok",
                         GameType::Dynamics::kSequential,
                         GameType::ChanceMode::kExplicitStochastic,
                         GameType::Information::kPerfectInformation,
                         GameType::Utility::kZeroSum,
                         GameType::RewardModel::kTerminal,
                         /*max_num_players=*/kNumPlayers,
                         /*min_num_players=*/kNumPlayers,
                         /*provides_information_state_string=*/false,
                         /*provides_information_state_tensor=*/false,
                         /*provides_observation_string=*/true,
                         /*provides_observation_tensor=*/true,
                         /*parameter_specification=*/
                         {}};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new HungarianTarokGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);
}  // namespace

namespace {

class HungarianTarokObserver final : public Observer {
 public:
  explicit HungarianTarokObserver(IIGObservationType iig_obs_type)
      : Observer(/*has_string=*/true, /*has_tensor=*/true),
        iig_obs_type_(iig_obs_type) {}

  ~HungarianTarokObserver() noexcept override = default;

  void WriteTensor(const State& observed_state, int player,
                   Allocator* allocator) const override {
    const auto& state =
        open_spiel::down_cast<const HungarianTarokState&>(observed_state);
    SPIEL_CHECK_GE(player, 0);
    SPIEL_CHECK_LT(player, state.NumPlayers());

    // Minimal placeholder observation:
    // - one-hot player id of the observer
    // - one-hot current player (or all-zero if terminal)
    // - one bit: chance_done
    auto player_out = allocator->Get("observer", {state.NumPlayers()});
    player_out.at(player) = 1;

    auto cur_out = allocator->Get("current_player", {state.NumPlayers()});
    if (!state.IsTerminal() && state.CurrentPlayer() >= 0 &&
        state.CurrentPlayer() < state.NumPlayers()) {
      cur_out.at(state.CurrentPlayer()) = 1;
    }

    auto phase_out = allocator->Get("phase", {1});
    // phase[0] == 1 once the initial chance action has happened.
    phase_out.at(0) = state.IsChanceNode() ? 0 : 1;
  }

  std::string StringFrom(const State& observed_state,
                         int player) const override {
    const auto& state =
        open_spiel::down_cast<const HungarianTarokState&>(observed_state);
    return absl::StrCat("observer=", player, " cur=", state.CurrentPlayer(),
                        " terminal=", state.IsTerminal());
  }

 private:
  IIGObservationType iig_obs_type_;
};

}  // namespace

HungarianTarokState::HungarianTarokState(std::shared_ptr<const Game> game)
    : State(std::move(game)), phase_(absl::make_unique<SetupPhase>()) {}

HungarianTarokState::HungarianTarokState(const HungarianTarokState& other)
    : State(other), phase_(other.phase_->Clone()) {}

int HungarianTarokState::CurrentPlayer() const {
  return phase_->CurrentPlayer();
}

void HungarianTarokState::DoApplyAction(Action move) {
  phase_->DoApplyAction(move);
  if (phase_->GameOver()) {
    return;
  }
  if (phase_->PhaseOver()) {
    phase_ = phase_->NextPhase();
  }
}

std::vector<Action> HungarianTarokState::LegalActions() const {
  return phase_->LegalActions();
}

std::string HungarianTarokState::ActionToString(Player player,
                                                Action move) const {
  return phase_->ActionToString(player, move);
}

std::string HungarianTarokState::ToString() const { return phase_->ToString(); }

bool HungarianTarokState::IsTerminal() const { return phase_->GameOver(); }

std::vector<double> HungarianTarokState::Returns() const {
  return phase_->Returns();
}

std::string HungarianTarokState::ObservationString(Player player) const {
  const auto& game = open_spiel::down_cast<const HungarianTarokGame&>(*game_);
  // Use the game's default observer.
  auto observer = game.MakeObserver(kDefaultObsType, /*params=*/{});
  return observer->StringFrom(*this, player);
}

void HungarianTarokState::ObservationTensor(Player player,
                                            absl::Span<float> values) const {
  std::fill(values.begin(), values.end(), 0.0f);
}

std::unique_ptr<State> HungarianTarokState::Clone() const {
  return std::unique_ptr<State>(new HungarianTarokState(*this));
}

std::vector<std::pair<Action, double>> HungarianTarokState::ChanceOutcomes()
    const {
  SPIEL_CHECK_TRUE(IsChanceNode());
  std::vector<std::pair<Action, double>> outcomes;
  std::vector<Action> legal_actions = LegalActions();
  double prob = 1.0 / static_cast<double>(legal_actions.size());
  for (Action action : legal_actions) {
    outcomes.push_back({action, prob});
  }
  return outcomes;
}

HungarianTarokGame::HungarianTarokGame(const GameParameters& params)
    : Game(kGameType, params) {}

std::unique_ptr<State> HungarianTarokGame::NewInitialState() const {
  return absl::make_unique<HungarianTarokState>(shared_from_this());
}

std::vector<int> HungarianTarokGame::ObservationTensorShape() const {
  return {1};
}

std::shared_ptr<Observer> HungarianTarokGame::MakeObserver(
    absl::optional<IIGObservationType> iig_obs_type,
    const GameParameters& params) const {
  if (params.empty()) {
    return std::make_shared<HungarianTarokObserver>(
        iig_obs_type.value_or(kDefaultObsType));
  }
  return MakeRegisteredObserver(iig_obs_type, params);
}

std::string HungarianTarokGame::ActionToString(Player player,
                                               Action action) const {
  if (player == kChancePlayerId) {
    return "Chance";
  }
  if (action == ActionType::kPass) return "Pass";
  return absl::StrCat("Action(", action, ")");
}

int HungarianTarokGame::NumPlayers() const { return kNumPlayers; }

int HungarianTarokGame::MaxGameLength() const {
  return 300;  // TODO
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
