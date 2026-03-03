#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

void HungarianTarokState::StartTalonPhase() {
  talon_ = TalonState{};

  const Player declarer = common_state_.declarer_;
  const int declarer_cards_to_take = common_state_.winning_bid_;

  talon_.cards_to_take_.fill(0);
  talon_.cards_to_take_[declarer] = declarer_cards_to_take;
  talon_.current_player_ = declarer;

  int remaining_cards = kTalonSize - declarer_cards_to_take;
  Player player = (declarer + 1) % kNumPlayers;
  while (remaining_cards > 0) {
    if (player != declarer) {
      talon_.cards_to_take_[player]++;
      remaining_cards--;
    }
    player = (player + 1) % kNumPlayers;
  }

  int index = 0;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == CardLocation::kTalon) {
      talon_.talon_cards_[index++] = card;
    }
  }
  SPIEL_CHECK_EQ(index, kTalonSize);
}

Player HungarianTarokState::TalonCurrentPlayer() const {
  return TalonPhaseOver() ? kTerminalPlayerId : kChancePlayerId;
}

std::vector<Action> HungarianTarokState::TalonLegalActions() const {
  SPIEL_CHECK_FALSE(TalonPhaseOver());
  std::vector<Action> actions;
  for (int i = 0; i < kTalonSize; ++i) {
    if (!talon_.talon_taken_[i]) {
      actions.push_back(i);
    }
  }
  return actions;
}

bool HungarianTarokState::TrialThreeGameEnded() {
  if (talon_.current_player_ == common_state_.declarer_ &&
      common_state_.trial_three_) {
    if (PlayerHoldsCard(common_state_.declarer_, kPagat) ||
        PlayerHoldsCard(common_state_.declarer_, kSkiz) ||
        PlayerHoldsCard(common_state_.declarer_, kXXI)) {
      // declarer took an honour, continue
      return false;
    } else {
      return true;
    }
  }
  return false;
}

void HungarianTarokState::TalonDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kTalonSize);
  SPIEL_CHECK_FALSE(talon_.talon_taken_[action]);
  SPIEL_CHECK_FALSE(TalonPhaseOver());

  talon_.talon_taken_[action] = true;
  common_state_.deck_[talon_.talon_cards_[action]] =
      PlayerHandLocation(talon_.current_player_);
  talon_.talon_taken_count_++;
  talon_.cards_to_take_[talon_.current_player_]--;

  if (talon_.talon_taken_count_ == kTalonSize) {
    talon_.current_player_ = kTerminalPlayerId;
  } else if (talon_.cards_to_take_[talon_.current_player_] == 0) {
    if (TrialThreeGameEnded()) {
      talon_.current_player_ = kTerminalPlayerId;
      talon_.rewards_.assign(kNumPlayers, +3.0);
      talon_.rewards_[common_state_.declarer_] = -9.0;
      talon_.game_over_ = true;
    } else {
      talon_.current_player_ = (talon_.current_player_ + 1) % kNumPlayers;
    }
  }
}

std::vector<double> HungarianTarokState::TalonReturns() const {
  return talon_.rewards_;
}

bool HungarianTarokState::TalonGameOver() const {
  return talon_.current_player_ == kTerminalPlayerId && talon_.game_over_;
}

bool HungarianTarokState::TalonPhaseOver() const {
  return talon_.current_player_ == kTerminalPlayerId;
}

std::string HungarianTarokState::TalonActionToString(Player player,
                                                     Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kTalonSize);
  return absl::StrCat("Take talon card ", action);
}

std::string HungarianTarokState::TalonToString() const {
  return "Dealing Talon Phase";
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
