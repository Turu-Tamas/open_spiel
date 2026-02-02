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

// Hungarian Tarok implementation.

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "card.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/abseil-cpp/absl/types/span.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/observer.h"
#include "open_spiel/spiel.h"
#include "phases.h"

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

  PhaseType GetPhaseType() const { return current_phase_; }

  std::vector<Card> PlayerHand(Player player) const;
  bool PlayerHoldsCard(Player player, Card card) const {
    return common_state_.deck_[card] == PlayerHandLocation(player);
  }

 protected:
  void DoApplyAction(Action move) override;

 private:
  // Phase dispatch.
  Player PhaseCurrentPlayer() const;
  std::vector<Action> PhaseLegalActions() const;
  void PhaseDoApplyAction(Action action);
  bool PhaseOver() const;
  bool GameOver() const;
  std::vector<double> PhaseReturns() const;
  std::string PhaseActionToString(Player player, Action action) const;
  std::string PhaseToString() const;
  void AdvancePhase();

  // Phase implementations.
  // Setup.
  Player SetupCurrentPlayer() const;
  std::vector<Action> SetupLegalActions() const;
  void SetupDoApplyAction(Action action);
  bool SetupPhaseOver() const;
  std::string SetupActionToString(Player player, Action action) const;
  std::string SetupToString() const;

  // Bidding.
  Player BiddingCurrentPlayer() const;
  std::vector<Action> BiddingLegalActions() const;
  void BiddingDoApplyAction(Action action);
  bool BiddingPhaseOver() const;
  bool BiddingGameOver() const;
  std::string BiddingActionToString(Player player, Action action) const;
  std::string BiddingToString() const;
  void StartBiddingPhase();
  void BiddingNextPlayer();

  // Talon dealing.
  Player TalonCurrentPlayer() const;
  std::vector<Action> TalonLegalActions() const;
  void TalonDoApplyAction(Action action);
  bool TalonPhaseOver() const;
  std::string TalonActionToString(Player player, Action action) const;
  std::string TalonToString() const;
  void StartTalonPhase();
  bool TrialThreeGameEnded();  // private helper
  std::vector<double> TalonReturns() const;
  bool TalonGameOver() const;

  // Skart.
  Player SkartCurrentPlayer() const;
  std::vector<Action> SkartLegalActions() const;
  void SkartDoApplyAction(Action action);
  bool SkartPhaseOver() const;
  std::string SkartActionToString(Player player, Action action) const;
  std::string SkartToString() const;
  void StartSkartPhase();

  // Announcements.
  Player AnnouncementsCurrentPlayer() const;
  std::vector<Action> AnnouncementsLegalActions() const;
  void AnnouncementsDoApplyAction(Action action);
  bool AnnouncementsPhaseOver() const;
  std::string AnnouncementsActionToString(Player player, Action action) const;
  std::string AnnouncementsToString() const;
  void StartAnnouncementsPhase();
  void AnnouncementsCallPartner(Action action);
  bool IsDeclarerSidePlayer(Player player) const;
  CommonState::AnnouncementSide& CurrentAnnouncementSide();
  CommonState::AnnouncementSide& OtherAnnouncementSide();
  const CommonState::AnnouncementSide& CurrentAnnouncementSide() const;
  const CommonState::AnnouncementSide& OtherAnnouncementSide() const;
  bool CanAnnounceTuletroa() const;

  // Play.
  Player PlayCurrentPlayer() const;
  std::vector<Action> PlayLegalActions() const;
  void PlayDoApplyAction(Action action);
  bool PlayPhaseOver() const;
  bool PlayGameOver() const;
  std::string PlayActionToString(Player player, Action action) const;
  std::string PlayToString() const;
  std::vector<double> PlayReturns() const;
  void StartPlayPhase();
  void ResolveTrick();

  // Persistent game data.
  CommonState common_state_{};
  PhaseType current_phase_ = PhaseType::kSetup;

  // Per-phase state.
  struct SetupState {
    std::array<int, kNumPlayers> player_hands_sizes{};
    Card current_card = 0;
  } setup_;

  struct BiddingState {
    Player current_player = 0;
    Bid winning_bid_ = Bid::NewInitialBid();
    bool all_passed = false;
    std::array<bool, kNumPlayers> can_bid{};
    std::array<bool, kNumPlayers> has_bid{};
    std::array<bool, kNumPlayers> has_passed{};
    BidType bid_type = BidType::kStandard;
  } bidding_;

  struct TalonState {
    Player current_player = 0;  // the current player receiving a card
    std::array<Card, kTalonSize> talon_cards{};
    std::array<bool, kTalonSize> talon_taken{};
    std::array<int, kNumPlayers> cards_to_take{};
    int talon_taken_count = 0;
    std::vector<double> rewards = {0.0, 0.0, 0.0, 0.0};
    bool game_over = false;
  } talon_;

  struct SkartState {
    Player current_player = 0;
    std::array<int, kNumPlayers> hand_sizes{};
    int cards_discarded = 0;
  } skart_;

  struct AnnouncementsState {
    Player current_player = 0;
    bool partner_called = false;
    Player last_to_speak = 0;
    bool first_round = true;
    std::array<int, kNumPlayers> tarok_counts{};
  } announcements_;

  struct PlayState {
    Player current_player = 0;
    Player trick_caller = 0;
    std::vector<Card> trick_cards;
    int round = 0;
  } play_;
};

class HungarianTarokGame : public Game {
 public:
  explicit HungarianTarokGame(const GameParameters& params);

  int NumDistinctActions() const override { return kDeckSize; }
  std::unique_ptr<State> NewInitialState() const override;
  int MaxChanceOutcomes() const override { return kDeckSize; }
  int NumPlayers() const override;
  double MinUtility() const override { return -100000; }
  double MaxUtility() const override { return +100000; }
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
