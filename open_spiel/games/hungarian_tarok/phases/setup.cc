#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

Player HungarianTarokState::SetupCurrentPlayer() const {
  return SetupPhaseOver() ? kTerminalPlayerId : kChancePlayerId;
}

std::vector<Action> HungarianTarokState::SetupLegalActions() const {
  SPIEL_CHECK_FALSE(SetupPhaseOver());
  std::vector<Action> actions;
  for (int player = 0; player < kNumPlayers; ++player) {
    if (setup_.player_hands_sizes_[player] < kPlayerHandSize) {
      actions.push_back(player);
    }
  }
  return actions;
}

void HungarianTarokState::SetupDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kNumPlayers);
  SPIEL_CHECK_FALSE(SetupPhaseOver());

  if (setup_.current_card_ == kPagat) {
    common_state_.pagat_holder_ = action;
  } else if (setup_.current_card_ == kXXI) {
    common_state_.XXI_holder_ = action;
  } else if (setup_.current_card_ == kSkiz) {
    common_state_.skiz_holder_ = action;
  }

  // Action is the player ID who receives the next card.
  common_state_.deck_[setup_.current_card_] = PlayerHandLocation(action);
  setup_.player_hands_sizes_[action]++;
  setup_.current_card_++;
}

bool HungarianTarokState::SetupPhaseOver() const {
  return setup_.current_card_ >= kPlayerHandSize * kNumPlayers;
}

std::string HungarianTarokState::SetupActionToString(Player player,
                                                     Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kNumPlayers);
  return absl::StrCat("Deal card ", CardToString(setup_.current_card_),
                      " to player ", action);
}

std::string HungarianTarokState::SetupToString() const { return "Setup Phase"; }

}  // namespace hungarian_tarok
}  // namespace open_spiel
