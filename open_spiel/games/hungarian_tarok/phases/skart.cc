#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

namespace open_spiel {
namespace hungarian_tarok {

void HungarianTarokState::StartSkartPhase() {
  skart_ = SkartState{};
  skart_.hand_sizes_.fill(0);
  for (Card card = 0; card < kDeckSize; ++card) {
    CardLocation location = common_state_.deck_[card];
    if (IsPlayerHand(location)) {
      skart_.hand_sizes_[HandLocationPlayer(location)]++;
    }
  }
  skart_.current_player_ = common_state_.declarer_;
}

Player HungarianTarokState::SkartCurrentPlayer() const {
  return skart_.current_player_;
}

std::vector<Action> HungarianTarokState::SkartLegalActions() const {
  SPIEL_CHECK_FALSE(SkartPhaseOver());
  std::vector<Action> actions;

  for (Card card = 0; card < kDeckSize; ++card) {
    const std::optional<Card> mandatory = common_state_.mandatory_called_card_;
    if (IsHonour(card)) continue;
    if (card == common_state_.mandatory_called_card_) continue;
    if (!mandatory.has_value() && card == MakeTarok(20)) continue;
    if (CardSuit(card) != Suit::kTarok && CardSuitRank(card) == SuitRank::kKing)
      continue;

    if (PlayerHoldsCard(skart_.current_player_, card)) {
      actions.push_back(card);
    }
  }
  return actions;
}

void HungarianTarokState::SkartDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  SPIEL_CHECK_TRUE(PlayerHoldsCard(skart_.current_player_, action));
  SPIEL_CHECK_FALSE(SkartPhaseOver());

  if (skart_.current_player_ == common_state_.declarer_) {
    common_state_.deck_[action] = CardLocation::kDeclarerSkart;
  } else {
    common_state_.deck_[action] = CardLocation::kOpponentsSkart;
  }
  skart_.cards_discarded_++;

  skart_.hand_sizes_[skart_.current_player_]--;
  if (skart_.hand_sizes_[skart_.current_player_] == kPlayerHandSize) {
    skart_.current_player_ = (skart_.current_player_ + 1) % kNumPlayers;
    if (skart_.hand_sizes_[skart_.current_player_] == 0) {
      skart_.current_player_ = kTerminalPlayerId;
    }
  }
}

bool HungarianTarokState::SkartPhaseOver() const {
  return skart_.cards_discarded_ == kTalonSize;
}

std::string HungarianTarokState::SkartActionToString(Player player,
                                                     Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  return absl::StrCat("Discard card ", CardToString(static_cast<Card>(action)));
}

std::string HungarianTarokState::SkartToString() const {
  return absl::StrCat("Skart Phase, ", skart_.cards_discarded_,
                      "/6 cards discarded\n",
                      DeckToString(common_state_.deck_));
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
