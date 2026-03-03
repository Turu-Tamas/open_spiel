#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"
#include "open_spiel/games/hungarian_tarok/scoring.h"

namespace open_spiel {
namespace hungarian_tarok {

void HungarianTarokState::StartPlayPhase() {
  play_ = PlayState{};
  play_.current_player_ = common_state_.declarer_;
  play_.trick_caller_ = play_.current_player_;
  play_.trick_cards_.clear();
  play_.round_ = 0;
  common_state_.tricks_.clear();
  common_state_.trick_winners_.clear();
}

Player HungarianTarokState::PlayCurrentPlayer() const {
  return play_.current_player_;
}

std::vector<Action> HungarianTarokState::PlayLegalActions() const {
  SPIEL_CHECK_FALSE(PlayPhaseOver());

  std::vector<Action> actions;
  auto can_play = [&](Card card) {
    return play_.trick_cards_.empty() ||
           CardSuit(play_.trick_cards_.front()) == CardSuit(card);
  };
  CardLocation current_player_hand = PlayerHandLocation(play_.current_player_);
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == current_player_hand && can_play(card)) {
      actions.push_back(card);
    }
  }
  if (!actions.empty()) return actions;

  bool has_tarok = false;
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] == current_player_hand &&
        CardSuit(card) == Suit::kTarok) {
      has_tarok = true;
      break;
    }
  }
  for (Card card = 0; card < kDeckSize; ++card) {
    if (common_state_.deck_[card] != current_player_hand) continue;
    if (!has_tarok || CardSuit(card) == Suit::kTarok) {
      actions.push_back(card);
    }
  }
  return actions;
}

void HungarianTarokState::PlayDoApplyAction(Action action) {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  SPIEL_CHECK_EQ(common_state_.deck_[action],
                 PlayerHandLocation(play_.current_player_));
  SPIEL_CHECK_FALSE(PlayPhaseOver());

  common_state_.deck_[action] = CardLocation::kCurrentTrick;
  play_.trick_cards_.push_back(action);
  if (play_.trick_cards_.size() == kNumPlayers) {
    ResolveTrick();
  } else {
    play_.current_player_ = (play_.current_player_ + 1) % kNumPlayers;
  }
}

void HungarianTarokState::ResolveTrick() {
  SPIEL_CHECK_EQ(play_.trick_cards_.size(), kNumPlayers);

  Player trick_winner = play_.trick_caller_;
  Card winning_card = play_.trick_cards_[0];
  for (int i = 1; i < kNumPlayers; ++i) {
    Card card = play_.trick_cards_[i];
    if (CardBeats(card, winning_card)) {
      winning_card = card;
      trick_winner = (play_.trick_caller_ + i) % kNumPlayers;
    }
  }

  play_.trick_caller_ = trick_winner;
  play_.current_player_ = trick_winner;
  for (Card card : play_.trick_cards_) {
    common_state_.deck_[card] = PlayerWonCardsLocation(trick_winner);
  }

  CommonState::Trick trick;
  absl::c_copy(play_.trick_cards_, trick.begin());
  common_state_.tricks_.push_back(trick);
  common_state_.trick_winners_.push_back(trick_winner);
  play_.trick_cards_.clear();

  play_.round_++;
  if (play_.round_ == kNumRounds) {
    play_.current_player_ = kTerminalPlayerId;
  }
}

bool HungarianTarokState::PlayPhaseOver() const {
  return play_.round_ >= kNumRounds;
}

bool HungarianTarokState::PlayGameOver() const { return PlayPhaseOver(); }

std::string HungarianTarokState::PlayActionToString(Player player,
                                                    Action action) const {
  SPIEL_CHECK_GE(action, 0);
  SPIEL_CHECK_LT(action, kDeckSize);
  return absl::StrCat("Play card ", CardToString(static_cast<Card>(action)));
}

std::string HungarianTarokState::PlayToString() const {
  return absl::StrCat("Play Phase, round ", play_.round_ + 1, " ",
                      play_.trick_cards_.size(), "/4 cards played\n",
                      DeckToString(common_state_.deck_));
}

std::vector<double> HungarianTarokState::PlayReturns() const {
  if (!PlayPhaseOver()) {
    return std::vector<double>(kNumPlayers, 0.0);
  }
  const std::array<int, kNumPlayers> scores =
      CalculateScores(MakeScoringSummary(common_state_));
  std::vector<double> returns;
  returns.reserve(kNumPlayers);
  for (int p = 0; p < kNumPlayers; ++p) {
    returns.push_back(static_cast<double>(scores[p]));
  }
  return returns;
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
