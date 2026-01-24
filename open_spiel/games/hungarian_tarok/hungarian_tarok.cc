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

#include "game_phase.h"
#include "setup.h"

namespace open_spiel {
namespace hungarian_tarok {
namespace {

const GameType kGameType{/*short_name=*/"hungarian_tarok",
                         /*long_name=*/"Hungarian Tarok (placeholder)",
                         GameType::Dynamics::kSequential,
                         GameType::ChanceMode::kExplicitStochastic,
                         GameType::Information::kPerfectInformation,
                         GameType::Utility::kGeneralSum,
                         GameType::RewardModel::kTerminal,
                         /*max_num_players=*/4,
                         /*min_num_players=*/2,
                         /*provides_information_state_string=*/false,
                         /*provides_information_state_tensor=*/false,
                         /*provides_observation_string=*/true,
                         /*provides_observation_tensor=*/true,
                         /*parameter_specification=*/
                         {{"players", GameParameter(4)}}};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new HungariantarokGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);
}  // namespace

namespace {

class HungariantarokObserver final : public Observer {
 public:
  explicit HungariantarokObserver(IIGObservationType iig_obs_type)
      : Observer(/*has_string=*/true, /*has_tensor=*/true),
        iig_obs_type_(iig_obs_type) {}

  void WriteTensor(const State& observed_state, int player,
                   Allocator* allocator) const override {
    const auto& state =
        open_spiel::down_cast<const HungariantarokState&>(observed_state);
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

    (void)iig_obs_type_;  // Placeholder: not using public/private flags yet.
  }

  std::string StringFrom(const State& observed_state,
                         int player) const override {
    const auto& state =
        open_spiel::down_cast<const HungariantarokState&>(observed_state);
    return absl::StrCat("observer=", player, " cur=", state.CurrentPlayer(),
                        " terminal=", state.IsTerminal());
  }

 private:
  IIGObservationType iig_obs_type_;
};

}  // namespace

HungariantarokState::HungariantarokState(std::shared_ptr<const Game> game)
    : State(std::move(game)),
      cur_player_(kChancePlayerId),
      step_(0),
      chance_done_(false) {}

int HungariantarokState::CurrentPlayer() const {
  if (IsTerminal()) {
    return kTerminalPlayerId;
  } else {
    return cur_player_;
  }
}

void HungariantarokState::DoApplyAction(Action move) {
  if (IsChanceNode()) {
    // Single placeholder chance action.
    SPIEL_CHECK_EQ(move, 0);
    chance_done_ = true;
    cur_player_ = 0;
    return;
  }

  // Player node: only Pass.
  SPIEL_CHECK_EQ(move, ActionType::kPass);
  step_++;
  if (step_ >= NumPlayers()) {
    cur_player_ = kTerminalPlayerId;
  } else {
    cur_player_ = step_;
  }
}

std::vector<Action> HungariantarokState::LegalActions() const {
  if (IsTerminal()) return {};
  if (IsChanceNode()) return {0};
  return {ActionType::kPass};
}

std::string HungariantarokState::ActionToString(Player player, Action move) const {
  return GetGame()->ActionToString(player, move);
}

std::string HungariantarokState::ToString() const {
  return absl::StrCat("chance_done=", chance_done_, " step=", step_,
                      " cur=", cur_player_);
}

bool HungariantarokState::IsTerminal() const {
  return chance_done_ && step_ >= NumPlayers();
}

std::vector<double> HungariantarokState::Returns() const {
  return std::vector<double>(NumPlayers(), 0.0);
}

std::string HungariantarokState::ObservationString(Player player) const {
  const auto& game =
      open_spiel::down_cast<const HungariantarokGame&>(*game_);
  // Use the game's default observer.
  auto observer = game.MakeObserver(kDefaultObsType, /*params=*/{});
  return observer->StringFrom(*this, player);
}

void HungariantarokState::ObservationTensor(Player player,
                                   absl::Span<float> values) const {
  std::fill(values.begin(), values.end(), 0.0f);
  if (player >= 0 && player < NumPlayers()) {
    values[player] = 1.0f;  // observer one-hot
  }
  // last element indicates whether chance is done.
  values[NumPlayers()] = chance_done_ ? 1.0f : 0.0f;
}

std::unique_ptr<State> HungariantarokState::Clone() const {
  return std::unique_ptr<State>(new HungariantarokState(*this));
}

std::vector<std::pair<Action, double>> HungariantarokState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(IsChanceNode());
  return {{0, 1.0}};
}

HungariantarokGame::HungariantarokGame(const GameParameters& params)
    : Game(kGameType, params),
      num_players_(ParameterValue<int>("players")) {
  SPIEL_CHECK_GE(num_players_, kGameType.min_num_players);
  SPIEL_CHECK_LE(num_players_, kGameType.max_num_players);
}

std::unique_ptr<State> HungariantarokGame::NewInitialState() const {
  return absl::make_unique<HungariantarokState>(shared_from_this());
}

std::vector<int> HungariantarokGame::ObservationTensorShape() const {
  // observer one-hot (num_players) + 1 phase bit.
  return {num_players_ + 1};
}

std::shared_ptr<Observer> HungariantarokGame::MakeObserver(
    absl::optional<IIGObservationType> iig_obs_type,
    const GameParameters& params) const {
  if (params.empty()) {
    return std::make_shared<HungariantarokObserver>(
        iig_obs_type.value_or(kDefaultObsType));
  }
  return MakeRegisteredObserver(iig_obs_type, params);
}

std::string HungariantarokGame::ActionToString(Player player, Action action) const {
  if (player == kChancePlayerId) {
    return "Chance";
  }
  if (action == ActionType::kPass) return "Pass";
  return absl::StrCat("Action(", action, ")");
}

int HungariantarokGame::NumPlayers() const { return num_players_; }

int HungariantarokGame::MaxGameLength() const {
  // 1 chance action + one action per player.
  return 1 + num_players_;
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
