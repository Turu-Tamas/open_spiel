#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/games/hungarian_tarokk/announcements.h"
#include "open_spiel/games/hungarian_tarokk/bidding.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/games/hungarian_tarokk/scoring.h"
#include "open_spiel/games/hungarian_tarokk/talon.h"
#include "open_spiel/spiel.h"

namespace open_spiel {
namespace hungarian_tarokk {

inline constexpr int kCardsDealtToPlayers = kNumPlayers * kHandSize;  // 36

// Comfortable upper bounds; the real maxima are lower.
inline constexpr int kMaxBiddingDecisions = 16;
inline constexpr int kMaxAnnouncementDecisions = 200;

// Action ids live in bidding.h (42..47), talon.h (48..91) and announcements.h
// (92..135); the total action count runs up to the last of them.
inline constexpr int kNumDistinctActions = kLastAnnounceAction + 1;  // 136

// The phases the parent state orchestrates. The auction, the talon exchange and
// the announcements are each a single phase delegated to their own
// sub-state-machine (bidding.h, talon.h, announcements.h); the parent itself
// only handles dealing, play and the transitions.
enum class Phase {
  kDealing,
  kBidding,
  kTalonExchange,
  kAnnouncements,
  kPlaying,
  kFinished
};

// The flat observation tensor length. The observation is a fixed-size,
// cumulative snapshot of what a player may see (rules.md §5.5, §6.3-§6.4).
// Because every public fact it holds is monotonic -- bids only rise,
// announcements only accumulate, discards and points only grow -- plus the one
// most-recent completed trick, a single snapshot always conveys everything that
// happened since the player's previous turn. In particular the whole auction
// (which player reached each bid-slot, and its outcome) persists into every
// later block, so a player skipped during the bidding still sees it here.
// Players are encoded relative to the observer (self = 0); the trailing slot of
// a player one-hot marked (+1) is "none". Index-valued blocks (the bid-slots,
// the kontra levels and the discard counts) hold raw values, not normalised
// ones. The blocks, in order:
//   phase(6) current-player(4) own-hand(42) declarer(5) bid(5) obligatory(4)
//   bidders(7) called-tarokk(23) sides(4x3) tarokk-declarations(4x3)
//   hivatalbol(5) bonus-announced(6x2) bonus-kontra(6x2) game-kontra(1)
//   discarded-tarokk-counts(4) declarer-shown-tarokks(42)
//   current-trick(4x43) current-trick-leader(4) last-trick(4x43)
inline constexpr int kObservationTensorSize =
    6 + kNumPlayers + kNumCards + (kNumPlayers + 1) + (kNumBids + 1) + 4 +
    (2 * kNumBids - 1) + (kNumTarokks + 1) + kNumPlayers * 3 + kNumPlayers * 3 +
    (kNumPlayers + 1) + kNumBonuses * 2 + kNumBonuses * 2 + 1 + kNumPlayers +
    kNumCards + kNumPlayers * (kNumCards + 1) + kNumPlayers +
    kNumPlayers * (kNumCards + 1);

class HungarianTarokkState : public State {
 public:
  explicit HungarianTarokkState(std::shared_ptr<const Game> game);
  HungarianTarokkState(const HungarianTarokkState&) = default;

  Player CurrentPlayer() const override;
  std::vector<Action> LegalActions() const override;
  std::string ActionToString(Player player, Action action_id) const override;
  std::string ToString() const override;
  bool IsTerminal() const override;
  std::vector<double> Returns() const override;
  std::string InformationStateString(Player player) const override;
  std::string ObservationString(Player player) const override;
  void ObservationTensor(Player player,
                         absl::Span<float> values) const override;
  std::unique_ptr<State> Clone() const override;
  ActionsAndProbs ChanceOutcomes() const override;

  Phase CurrentPhase() const { return phase_; }
  std::vector<Card> PlayerCards(Player player) const {
    return CurrentHands()[player];
  }
  std::vector<Card> Talon() const { return CurrentTalon(); }
  const BiddingState& Bidding() const { return bidding_; }
  // kInvalidPlayer until the auction has produced a declarer (and stays so for
  // a passed-out hand).
  Player Declarer() const { return declarer_; }
  Bid WinningBid() const { return winning_bid_; }
  // True if a player threw the hand in during the annulment phase.
  bool IsAnnulled() const { return annulled_; }
  const AnnouncementState& Announcements() const { return announcements_; }
  // The declarer's partner (holder of the called tarokk), or kInvalidPlayer
  // when the declarer plays alone; valid once the announcements are done.
  Player Partner() const { return partner_; }

 protected:
  void DoApplyAction(Action action_id) override;

 private:
  std::vector<Action> LegalPlayActions() const;
  void ResolveTrick();
  void StartPlaying();
  // Gathers the completed deal's trick-play and announcement facts for scoring
  // (§7). Valid only once all nine tricks are played.
  DealScore BuildDealScore() const;
  std::string PublicLogString() const;
  // The hands / skart / talon currently live inside talon_exchange_ during the
  // exchange and in the parent's own members otherwise; these resolve to the
  // right one.
  const std::vector<std::vector<Card>>& CurrentHands() const;
  const std::vector<std::vector<Card>>& CurrentDiscards() const;
  const std::vector<Card>& CurrentTalon() const;

  std::vector<PlayerAction>::const_iterator BiddingHistoryBegin() const {
    return bidding_history_start_ ? history_.cbegin() + *bidding_history_start_
                                  : history_.cend();
  }
  std::vector<PlayerAction>::const_iterator BiddingHistoryEnd() const {
    return bidding_history_end_ ? history_.cbegin() + *bidding_history_end_
                                : history_.cend();
  }

  Phase phase_ = Phase::kDealing;
  Player current_player_ = kChancePlayerId;
  int cards_dealt_ = 0;
  std::vector<Card> deck_;                        // undealt cards (sorted)
  std::vector<std::vector<Card>> players_cards_;  // hands (outside the talon)
  std::vector<Card> talon_;                       // talon (outside the talon)
  BiddingState bidding_;
  std::optional<int>
      bidding_history_start_;  // index of the first bidding action in history_
  std::optional<int>
      bidding_history_end_;  // index of the last bidding action in history_
  Player declarer_ = kInvalidPlayer;
  Bid winning_bid_ = Bid::kThree;
  TalonExchangeState talon_exchange_;
  std::vector<std::vector<Card>> discarded_;  // each player's skart, post-talon
  bool annulled_ = false;
  AnnouncementState announcements_;
  Card called_card_ = kInvalidCard;  // the tarokk the declarer called
  Player partner_ = kInvalidPlayer;
  std::array<int, kNumPlayers> collected_points_;
  Player trick_leader_ = 0;
  std::vector<Card> trick_cards_;  // current (partial) trick
  std::vector<std::vector<Card>> completed_tricks_;
  std::vector<Player> trick_winners_;  // winner of each completed trick
  int tricks_played_ = 0;
};

class HungarianTarokkGame : public Game {
 public:
  explicit HungarianTarokkGame(const GameParameters& params);
  int NumDistinctActions() const override { return kNumDistinctActions; }
  std::unique_ptr<State> NewInitialState() const override {
    return std::unique_ptr<State>(new HungarianTarokkState(shared_from_this()));
  }
  int MaxChanceOutcomes() const override { return kNumCards; }
  int NumPlayers() const override { return kNumPlayers; }
  std::vector<int> ObservationTensorShape() const override {
    return {kObservationTensorSize};
  }
  double MinUtility() const override { return -kMaxDealScore; }
  double MaxUtility() const override { return kMaxDealScore; }
  absl::optional<double> UtilitySum() const override { return 0.0; }
  int MaxGameLength() const override {
    // Bidding + annulment (<= one per player) + the six discards + the round of
    // announcements + the nine tricks.
    return kMaxBiddingDecisions + kNumPlayers + kTalonSize +
           kMaxAnnouncementDecisions + kCardsDealtToPlayers;
  }
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_
