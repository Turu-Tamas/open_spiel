#include "open_spiel/games/hungarian_tarokk/talon.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarokk/bidding.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarokk {

TalonExchangeState::TalonExchangeState(std::vector<std::vector<Card>> hands,
                                       std::vector<Card> talon, Player declarer,
                                       Bid bid, Player cue_bidder,
                                       CalledCard cued_card)
    : hands_(std::move(hands)),
      talon_(std::move(talon)),
      declarer_(declarer),
      cue_bidder_(cue_bidder),
      cued_card_(cued_card) {
  SPIEL_CHECK_EQ(kTalonSize, talon_.size());
  discards_.resize(kNumPlayers);
  draw_counts_.assign(kNumPlayers, 0);
  discards_remaining_.assign(kNumPlayers, 0);
  // The declarer draws BidTalonExchange(bid) cards; the rest are split among
  // the other players in order from the declarer's right, the nearest taking
  // the extra (rules.md §4.1).
  const int declarer_count = BidTalonExchange(bid);
  const int others_total = static_cast<int>(talon_.size()) - declarer_count;
  const int base = others_total / (kNumPlayers - 1);
  const int remainder = others_total % (kNumPlayers - 1);
  draw_counts_[declarer_] = declarer_count;
  for (int i = 1; i < kNumPlayers; ++i) {
    const Player p = (declarer_ + i) % kNumPlayers;
    draw_counts_[p] = base + (i <= remainder ? 1 : 0);
  }

  for (int i = 0; i < kNumPlayers; ++i) {
    const Player p = (declarer_ + i) % kNumPlayers;
    for (int j = 0; j < draw_counts_[p]; ++j) recipients_.push_back(p);
  }
}

Player TalonExchangeState::CurrentPlayer() const {
  return step_ == Step::kDraw ? kChancePlayerId : current_player_;
}

ActionsAndProbs TalonExchangeState::ChanceOutcomes() const {
  SPIEL_CHECK_TRUE(step_ == Step::kDraw);
  ActionsAndProbs outcomes;
  outcomes.reserve(talon_.size());
  const double p = 1.0 / static_cast<double>(talon_.size());
  for (Card card : talon_) outcomes.emplace_back(CardToAction(card), p);
  return outcomes;
}

std::vector<Action> TalonExchangeState::LegalActions() const {
  if (step_ == Step::kDraw) {
    return CardsToActions(talon_);  // chance: undrawn talon (sorted)
  }
  if (step_ == Step::kAnnul) {
    return {kActionAnnul, kActionDeclineAnnul};
  }
  if (step_ == Step::kDiscard) {
    std::vector<Action> legal;
    for (Card c : hands_[current_player_]) {
      if (CanDiscard(current_player_, c)) {
        legal.push_back(DiscardActionForCard(c));
      }
    }
    std::sort(legal.begin(), legal.end());
    return legal;
  }
  return {};
}

void TalonExchangeState::ApplyAction(Action action) {
  if (step_ == Step::kDraw) {
    const Card card = CardFromAction(action);
    auto it = std::find(talon_.begin(), talon_.end(), card);
    SPIEL_CHECK_TRUE(it != talon_.end());
    talon_.erase(it);
    hands_[recipients_[draw_index_]].push_back(card);
    ++draw_index_;
    if (draw_index_ == static_cast<int>(recipients_.size())) {
      for (std::vector<Card>& hand : hands_) {
        std::sort(hand.begin(), hand.end());
      }
      StartAnnulment();  // a player may throw the hand in before discarding
    }
    return;
  }

  if (step_ == Step::kAnnul) {
    if (action == kActionAnnul) {
      annulled_ = true;
      step_ = Step::kDone;
    } else {
      SPIEL_CHECK_EQ(action, kActionDeclineAnnul);
      ++annul_pos_;
      AdvanceAnnulment();
    }
    return;
  }

  SPIEL_CHECK_TRUE(step_ == Step::kDiscard);
  const Card card = CardForDiscardAction(action);
  std::vector<Card>& hand = hands_[current_player_];
  auto it = std::find(hand.begin(), hand.end(), card);
  SPIEL_CHECK_TRUE(it != hand.end());
  hand.erase(it);
  discards_[current_player_].push_back(card);
  --discards_remaining_[current_player_];
  AdvanceDiscarding();
}

void TalonExchangeState::StartAnnulment() {
  annul_pos_ = 0;
  step_ = Step::kAnnul;
  AdvanceAnnulment();
}

void TalonExchangeState::AdvanceAnnulment() {
  // Offer the throw-in to each player able to annul, the declarer first.
  while (annul_pos_ < kNumPlayers) {
    const Player p = (declarer_ + annul_pos_) % kNumPlayers;
    if (HandIsAnnullable(hands_[p])) {
      current_player_ = p;
      return;
    }
    ++annul_pos_;
  }
  StartDiscarding();  // nobody can (or chose to) annul
}

void TalonExchangeState::StartDiscarding() {
  for (int p = 0; p < kNumPlayers; ++p) {
    discards_remaining_[p] = draw_counts_[p];
  }
  discard_pos_ = 0;
  step_ = Step::kDiscard;
  AdvanceDiscarding();
}

void TalonExchangeState::AdvanceDiscarding() {
  // Each player who drew discards back down to nine, the declarer first.
  while (discard_pos_ < kNumPlayers) {
    const Player p = (declarer_ + discard_pos_) % kNumPlayers;
    if (discards_remaining_[p] > 0) {
      current_player_ = p;
      return;
    }
    ++discard_pos_;
  }
  step_ = Step::kDone;
}

bool TalonExchangeState::CanDiscard(Player player, Card card) const {
  if (!IsDiscardableCard(card)) return false;
  // The cue-bidder may not discard the tarokk they promised.
  if (cue_bidder_ == player) {
    if (cued_card_ == CalledCard::kXIX && card == kCardXIX) return false;
    if (cued_card_ == CalledCard::kXVIII && card == kCardXVIII) return false;
  }
  return true;
}

std::string TalonExchangeState::ToString() const {
  switch (step_) {
    case Step::kDraw:
      return absl::StrCat("drawing (", talon_.size(), " left)");
    case Step::kAnnul:
      return absl::StrCat("annul? P", current_player_);
    case Step::kDiscard:
      return absl::StrCat("discarding P", current_player_);
    case Step::kDone:
      return annulled_ ? "annulled" : "done";
  }
  SpielFatalError("Unknown talon step.");
}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
