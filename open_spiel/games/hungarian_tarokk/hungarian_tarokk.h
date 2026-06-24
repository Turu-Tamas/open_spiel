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

// A bare-bones implementation of Hungarian Tarokk (Paskievics / XX-hívásos).
// The full rules to be implemented are described in rules.md in this directory.
//
// This skeleton implements only the core mechanics so that the game builds,
// runs and passes the standard random-simulation tests:
//   * the 42-card tarokk pack and its card-point values,
//   * dealing 9 cards to each of the 4 players (the 6-card talon is set aside
//     and is currently unused),
//   * the bidding / auction (see bidding.h) that determines the declarer and
//     the contract,
//   * trick-taking play with the follow-suit / must-play-a-tarokk rules,
//   * card-point scoring at the end of the 9 tricks.
//
// The following (more complex) features are intentionally left for later and
// are NOT modelled yet:
//   * the conventional bids -- cue bids / invitations and yielded games -- and
//     the 4th-seat trial bid (see bidding.h),
//   * the talon exchange and discarding,
//   * calling a partner (so there are currently no teams / partnerships): the
//     declarer and winning bid are recorded but do not yet affect scoring,
//   * announcements and bonuses (trull, four kings, pagátultimó, XXI-catch,
//     double game, volát), kontra, and the associated scoring.
//
// Because partnerships are not modelled yet, scoring is a placeholder: every
// player individually collects the card points of the tricks they win, and the
// per-player return is those points minus the table average (a zero-sum
// result). The forehand (player 0) leads the first trick.

namespace open_spiel {
namespace hungarian_tarokk {

// The card / deck definitions (the 42-card pack, point values, names and trick
// comparison) live in cards.h; the bidding (auction) lives in bidding.h.
inline constexpr int kNumPlayers = 4;
inline constexpr int kNumTricks = 9;
inline constexpr int kCardsDealtToPlayers = kNumPlayers * kHandSize;  // 36

// A comfortable upper bound on the number of player decisions in the auction
// (each raise strictly increases the bid and each bid is held at most once, so
// the true maximum is well below this). Only used to bound MaxGameLength().
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

  // Bare-bones game-specific accessors (handy for tests / later features).
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
  std::vector<Action> trick_cards_;                 // current (partial) trick
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
