#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

Player HungarianTarokState::AnnulmentsCurrentPlayer() const {
  return annulments_.current_player_;
}

std::vector<Action> HungarianTarokState::AnnulmentsLegalActions() const {
  SPIEL_CHECK_FALSE(AnnulmentsPhaseOver());
  std::vector<Action> actions = {kDontAnnul};
  std::vector<Card> hand = PlayerHand(annulments_.current_player_);
  int tarok_count = absl::c_count_if(hand, [](Card card) {
    return CardSuit(card) == Suit::kTarok && card != kXXI && card != kPagat;
  });
  int king_count = absl::c_count_if(hand, [](Card card) {
    return CardSuit(card) != Suit::kTarok &&
           CardSuitRank(card) == SuitRank::kKing;
  });
  if (tarok_count == 0) actions.push_back(kAnnulTaroks);
  if (king_count == 4) actions.push_back(kAnnulKings);
  return actions;
}

void HungarianTarokState::AnnulmentsDoApplyAction(Action action) {
  SPIEL_CHECK_FALSE(AnnulmentsPhaseOver());
  std::vector<Action> legal_actions = AnnulmentsLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  if (action == kDontAnnul) {
    annulments_.current_player_ =
        (annulments_.current_player_ + 1) % kNumPlayers;
    if (annulments_.current_player_ == 0) {
      // all players had the chance to annul, move on
      annulments_.current_player_ = kTerminalPlayerId;
    }
    return;
  }
  annulments_.annulment_called_ = true;
  annulments_.current_player_ = kTerminalPlayerId;
}

bool HungarianTarokState::AnnulmentsPhaseOver() const {
  return annulments_.current_player_ == kTerminalPlayerId;
}

std::string HungarianTarokState::AnnulmentsActionToString(Player player,
                                                          Action action) const {
  SPIEL_CHECK_FALSE(AnnulmentsPhaseOver());
  std::vector<Action> legal_actions = AnnulmentsLegalActions();
  SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

  switch (action) {
    case kDontAnnul:
      return "Don't annul";
    case kAnnulTaroks:
      return "Annul without taroks";
    case kAnnulKings:
      return "Annul with four kings";
    default:
      SpielFatalError("Unknown action");
  }
}

bool HungarianTarokState::AnnulmentsGameOver() const {
  return annulments_.annulment_called_;
}

std::string HungarianTarokState::AnnulmentsToString() const {
  return "Annulments Phase";
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
