#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_H_

#include <array>
#include <memory>
#include <optional>
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
// (92..150); the total action count runs up to the last of them.
inline constexpr int kNumDistinctActions = kLastAnnounceAction + 1;  // 151

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

// A structured form of the observation. It carries exactly the facts
// ObservationTensor encodes (see the layout comment above), as named fields of
// machine-readable integer codes instead of a flat float vector. Cards are
// their 0..41 index (tarokks 0..21); players their absolute seat 0..3; phases,
// bids, sides (0 = declarers, 1 = defenders), the obligatory call and bonuses
// their respective enum values. A uniform -1 marks any absent / not-yet-known
// value (no bid, no called card, unknown side, an empty trick slot, ...).
// `observing_player` records whose observation this is. Only what the observer
// may see is filled in: their own hand alone, the publicly-known sides, the
// declarer's face-up skart, and so on -- matching the tensor.

// One announced bonus (§5.2) together with its kontra chain (§5.3).
struct HungarianTarokkBonusAnnouncement {
  int bonus;         // Bonus enum: 0 = Trull ... 5 = Volat
  int side;          // Side enum: 0 = declarers, 1 = defenders
  int kontra_level;  // 0 = announced (no kontra), 1 = kontra, 2 = rekontra, ...
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(HungarianTarokkBonusAnnouncement, bonus, side,
                                 kontra_level);
};

struct HungarianTarokkObservationContents {
  int phase;              // Phase enum: 0 = Dealing ... 5 = Finished
  int current_player;     // whose turn (0..3, or -1 when nobody's)
  std::vector<int> hand;  // the observer's own cards, by 0..41 index

  // Auction.
  int declarer;         // 0..3, or -1 until the auction decides one
  int bid;              // Bid enum -- the winning bid, or the standing bid while
                        // bidding; -1 before anyone has bid
  int obligatory_call;  // CalledCard enum: 0 = none, 1 = XIX, 2 = XVIII, 3 = XX
  // Which player (0..3, or -1) last reached each of the seven bid-slots, in
  // order: bid-3, bid-2, hold-2, bid-1, hold-1, bid-solo, hold-solo.
  std::vector<int> bid_slots;

  // Announcements (defaults until that phase is reached).
  int called_tarokk;                  // the called partner card (0..21), or -1
  std::vector<int> sides;             // per player: 0 declarers, 1 defenders,
                                      // -1 unknown
  std::vector<int> declared_tarokks;  // per player: 0, 8 or 9 (tarokkszám)
  int hivatalbol_kontra;              // player 0..3 who kontra'd by office,
                                      // or -1
  std::vector<HungarianTarokkBonusAnnouncement> bonus_announcements;
  int game_kontra;  // 0 = none, 1 = kontra, 2 = rekontra..

  // Talon / skart and trick play.
  std::vector<int> discard_tarokk_counts;   // per player: tarokks in skart
  std::vector<int> declarer_shown_tarokks;  // face-up skart tarokks (§6.4)
  // The card each player has played in the trick in progress (-1 = not yet),
  // the player leading it (-1 if none), and likewise for the last completed
  // trick. Both are indexed by absolute seat.
  std::vector<int> current_trick;
  int current_trick_leader;
  std::vector<int> last_trick;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(HungarianTarokkObservationContents, phase,
                                 current_player, hand, declarer, bid,
                                 obligatory_call, bid_slots, called_tarokk,
                                 sides, declared_tarokks, hivatalbol_kontra,
                                 bonus_announcements, game_kontra,
                                 discard_tarokk_counts, declarer_shown_tarokks,
                                 current_trick, current_trick_leader,
                                 last_trick);
};

struct HungarianTarokkObservationStruct : ObservationStruct,
                                          HungarianTarokkObservationContents {
  HungarianTarokkObservationStruct() = default;
  explicit HungarianTarokkObservationStruct(const std::string& json_str)
      : HungarianTarokkObservationStruct(nlohmann::json::parse(json_str)) {}
  explicit HungarianTarokkObservationStruct(const nlohmann::json& data) {
    data.get_to(static_cast<HungarianTarokkObservationContents&>(*this));
    observing_player = data.value("observing_player", -1);
  }
  nlohmann::json to_json_base() const override {
    nlohmann::json j =
        static_cast<const HungarianTarokkObservationContents&>(*this);
    j["observing_player"] = observing_player;
    return j;
  }
  int observing_player = -1;
};

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
  std::unique_ptr<ObservationStruct> ToObservationStruct(
      Player player) const override;
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
