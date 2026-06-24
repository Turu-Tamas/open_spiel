#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/games/hungarian_tarokk/bidding.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/spiel.h"

namespace open_spiel {
namespace hungarian_tarokk {

inline constexpr int kNumPlayers = 4;
inline constexpr int kNumTricks = 9;
inline constexpr int kCardsDealtToPlayers = kNumPlayers * kHandSize;  // 36

// A comfortable upper bound, the maximum is lower.
inline constexpr int kMaxBiddingDecisions = 16;

enum class Phase { kDealing, kBidding, kPlaying, kFinished };

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
  std::unique_ptr<State> Clone() const override;
  ActionsAndProbs ChanceOutcomes() const override;

  Phase CurrentPhase() const { return phase_; }
  std::vector<Action> PlayerCards(Player player) const {
    return players_cards_[player];
  }
  std::vector<Action> Talon() const { return talon_; }
  const BiddingState& Bidding() const { return bidding_; }
  // kInvalidPlayer until the auction has produced a declarer (and stays so for
  // a passed-out hand).
  Player Declarer() const { return declarer_; }
  Bid WinningBid() const { return winning_bid_; }

 protected:
  void DoApplyAction(Action action_id) override;

 private:
  std::vector<Action> LegalPlayActions() const;
  void ResolveTrick();
  std::string PublicLogString() const;

  Phase phase_ = Phase::kDealing;
  Player current_player_ = kChancePlayerId;
  int cards_dealt_ = 0;
  std::vector<Action> deck_;                        // undealt cards (sorted)
  std::vector<std::vector<Action>> players_cards_;  // current hands
  std::vector<Action> talon_;                       // 6 unused cards
  BiddingState bidding_;
  Player declarer_ = kInvalidPlayer;
  Bid winning_bid_ = Bid::kThree;
  std::array<int, kNumPlayers> collected_points_;
  Player trick_leader_ = 0;
  std::vector<Action> trick_cards_;  // current (partial) trick
  std::vector<std::vector<Action>> completed_tricks_;
  int tricks_played_ = 0;
};

class HungarianTarokkGame : public Game {
 public:
  explicit HungarianTarokkGame(const GameParameters& params);
  int NumDistinctActions() const override {
    return kNumCards + kNumBiddingActions;
  }
  std::unique_ptr<State> NewInitialState() const override {
    return std::unique_ptr<State>(new HungarianTarokkState(shared_from_this()));
  }
  int MaxChanceOutcomes() const override { return kNumCards; }
  int NumPlayers() const override { return kNumPlayers; }
  double MinUtility() const override { return -kTotalCardPoints; }
  double MaxUtility() const override { return kTotalCardPoints; }
  absl::optional<double> UtilitySum() const override { return 0.0; }
  int MaxGameLength() const override {
    return kMaxBiddingDecisions + kCardsDealtToPlayers;
  }
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_
