#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_TALON_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_TALON_H_

#include <string>
#include <vector>

#include "open_spiel/games/hungarian_tarokk/bidding.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/spiel.h"

// The talon exchange of Hungarian Tarokk (rules.md §4): drawing the talon
// cards, the optional throw-in (annulment) and discarding back down to nine.

namespace open_spiel {
namespace hungarian_tarokk {

// Action ids for the talon exchange, in their own disjoint range right after
// the bidding actions (42..47):
//   discarding card c -> kDiscardActionBase + c          (48..89)
//   annulling / declining to annul a hand                (90, 91)
inline constexpr Action kDiscardActionBase =
    kBiddingActionBase + kNumBiddingActions;
inline constexpr Action kActionAnnul = kDiscardActionBase + kNumCards;
inline constexpr Action kActionDeclineAnnul = kActionAnnul + 1;

inline bool IsDiscardAction(Action action) {
  return action >= kDiscardActionBase &&
         action < kDiscardActionBase + kNumCards;
}
inline Action DiscardActionForCard(Action card) {
  return kDiscardActionBase + card;
}
inline Action CardForDiscardAction(Action action) {
  return action - kDiscardActionBase;
}

class TalonExchangeState {
 public:
  TalonExchangeState() = default;
  // `hands` are the players' nine-card hands and `talon` the undrawn talon. The
  // declarer and bid fix who draws how many; the cue-bidder may not discard the
  // tarokk they promised (cued_card).
  TalonExchangeState(std::vector<std::vector<Action>> hands,
                     std::vector<Action> talon, Player declarer, Bid bid,
                     Player cue_bidder, CalledCard cued_card);

  Player CurrentPlayer() const;
  ActionsAndProbs ChanceOutcomes() const;  // the talon draw
  std::vector<Action> LegalActions() const;
  void ApplyAction(Action action);

  bool IsFinished() const { return step_ == Step::kDone; }
  bool Annulled() const { return annulled_; }
  const std::vector<std::vector<Action>>& Hands() const { return hands_; }
  const std::vector<std::vector<Action>>& Discards() const { return discards_; }
  const std::vector<Action>& Talon() const { return talon_; }

  std::string ToString() const;

 private:
  enum class Step { kDraw, kAnnul, kDiscard, kDone };

  bool CanDiscard(Player player, Action card) const;
  void StartAnnulment();
  void AdvanceAnnulment();
  void StartDiscarding();
  void AdvanceDiscarding();

  std::vector<std::vector<Action>> hands_;
  std::vector<Action> talon_;
  std::vector<std::vector<Action>> discards_;
  Player declarer_ = kInvalidPlayer;
  Player cue_bidder_ = kInvalidPlayer;
  CalledCard cued_card_ = CalledCard::kNone;

  Step step_ = Step::kDraw;
  Player current_player_ = kChancePlayerId;
  std::vector<Player> recipients_;  // who draws each talon card, in order
  int draw_index_ = 0;              // talon cards drawn so far
  std::vector<int> draw_counts_;    // cards each player draws (and discards)
  std::vector<int> discards_remaining_;
  int annul_pos_ = 0;    // position in the declarer-first annulment order
  int discard_pos_ = 0;  // position in the declarer-first discard order
  bool annulled_ = false;
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_TALON_H_
