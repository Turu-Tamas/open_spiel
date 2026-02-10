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
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/memory/memory.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_globals.h"
#include "open_spiel/spiel_utils.h"

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
  return std::make_shared<HungarianTarokGame>(params);
}

REGISTER_SPIEL_GAME(kGameType, Factory);

RegisterSingleTensorObserver single_tensor(kGameType.short_name);
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
    : State(std::move(game)) {
  current_phase_ = PhaseType::kSetup;

  // Initialize persistent game state.
  common_state_.deck_.fill(CardLocation::kTalon);
  common_state_.pagat_holder_ = -1;
  common_state_.declarer_ = 0;
  common_state_.winning_bid_ = -1;
  common_state_.full_bid_ = false;
  common_state_.partner_ = std::nullopt;
  common_state_.declarer_side_ = CommonState::AnnouncementSide{};
  common_state_.opponents_side_ = CommonState::AnnouncementSide{};
  common_state_.player_sides_.fill(Side::kOpponents);
  common_state_.tricks_.clear();
  common_state_.trick_winners_.clear();

  // Initialize setup phase state.
  setup_.player_hands_sizes_.fill(0);
  setup_.current_card_ = 0;
}

HungarianTarokState::HungarianTarokState(const HungarianTarokState& other)
    : State(other),
      common_state_(other.common_state_),
      current_phase_(other.current_phase_),
      setup_(other.setup_),
      bidding_(other.bidding_),
      talon_(other.talon_),
      skart_(other.skart_),
      announcements_(other.announcements_),
      play_(other.play_) {}

Player HungarianTarokState::CurrentPlayer() const {
  return PhaseCurrentPlayer();
}

void HungarianTarokState::DoApplyAction(Action move) {
  SPIEL_CHECK_FALSE(IsTerminal());
  PhaseDoApplyAction(move);
  if (GameOver()) return;
  if (PhaseOver()) AdvancePhase();
}

std::vector<Action> HungarianTarokState::LegalActions() const {
  return PhaseLegalActions();
}

std::string HungarianTarokState::ActionToString(Player player,
                                                Action move) const {
  return PhaseActionToString(player, move);
}

std::string HungarianTarokState::ToString() const { return PhaseToString(); }

bool HungarianTarokState::IsTerminal() const { return GameOver(); }

std::vector<double> HungarianTarokState::Returns() const {
  return PhaseReturns();
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
  return absl::StrCat("Action(", action, ")");
}

int HungarianTarokGame::NumPlayers() const { return kNumPlayers; }

int HungarianTarokGame::MaxGameLength() const {
  return 300;  // TODO
}

std::vector<Card> HungarianTarokState::PlayerHand(Player player) const {
  std::vector<Card> hand;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == PlayerHandLocation(player)) {
      hand.push_back(card);
    }
  }
  return hand;
}

HungarianTarokState DealHelper::PostSetup() {
  auto game = LoadGame("hungarian_tarok");
  HungarianTarokState state(game);

  const int total_dealt_cards = kPlayerHandSize * kNumPlayers;

  std::array<int, kNumPlayers> destined_remaining{};
  destined_remaining.fill(0);
  for (Card card = 0; card < total_dealt_cards; ++card) {
    if (card_destinations_[card].has_value()) {
      destined_remaining[card_destinations_[card].value()]++;
    }
  }

  std::array<int, kNumPlayers> current_card_counts{};
  current_card_counts.fill(0);

  Card current_card = 0;
  while (state.GetPhaseType() == PhaseType::kSetup) {
    auto legal_actions = state.LegalActions();

    Player target_player;
    if (card_destinations_[current_card].has_value()) {
      target_player = card_destinations_[current_card].value();
      destined_remaining[target_player]--;

      if (absl::c_find(legal_actions, static_cast<Action>(target_player)) ==
          legal_actions.end()) {
        SpielFatalError(
            "DealHelper: Cannot deal card to requested player - hand is full");
      }
    } else {
      target_player = -1;
      for (Action action : legal_actions) {
        Player p = static_cast<Player>(action);
        int room = kPlayerHandSize - current_card_counts[p];
        if (room > destined_remaining[p]) {
          target_player = p;
          break;
        }
      }
      if (target_player == -1) {  // should be impossible
        SpielFatalError(
            "DealHelper: Cannot deal card to any player - all hands full");
      }
    }

    state.ApplyAction(target_player);
    current_card_counts[target_player]++;
    current_card++;
  }

  return state;
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
