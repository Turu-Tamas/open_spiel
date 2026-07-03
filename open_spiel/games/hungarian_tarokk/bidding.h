#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_BIDDING_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_BIDDING_H_

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/spiel.h"

namespace open_spiel {
namespace hungarian_tarokk {

enum class Bid { kThree, kTwo, kOne, kSolo };
inline constexpr int kNumBids = 4;
inline constexpr Bid kWeakestBid = Bid::kThree;
inline constexpr Bid kStrongestBid = Bid::kSolo;
inline constexpr std::array<Bid, kNumBids> kAllBids = {Bid::kThree, Bid::kTwo,
                                                       Bid::kOne, Bid::kSolo};

bool BidStronger(Bid a, Bid b);
int BidTalonExchange(Bid bid);
int BidGameValue(Bid bid);
std::string BidToString(Bid bid);

// The tarokk that a cue bid or a yield obliges the declarer to call.
enum class CalledCard { kNone, kXIX, kXVIII, kXX };
std::string CalledCardToString(CalledCard card);
std::ostream& operator<<(std::ostream& os, CalledCard card);

struct PlayerBidInfo {
  bool has_honour = false;       // pagát, XXI or the Skíz
  bool has_high_honour = false;  // XXI or the Skíz
  bool has_xx = false;
  bool has_xix = false;
  bool has_xviii = false;
};

// Action ids for the auction. They are placed immediately after the 42 card
// actions; hungarian_tarokk.cc static_asserts that kBiddingActionBase equals
// the game's kNumCards so the two action ranges never overlap.
inline constexpr Action kBiddingActionBase = 42;
inline constexpr Action kActionPass = kBiddingActionBase + 0;
inline constexpr Action kActionBidThree = kBiddingActionBase + 1;
inline constexpr Action kActionBidTwo = kBiddingActionBase + 2;
inline constexpr Action kActionBidOne = kBiddingActionBase + 3;
inline constexpr Action kActionBidSolo = kBiddingActionBase + 4;
inline constexpr Action kActionHold = kBiddingActionBase + 5;
inline constexpr int kNumBiddingActions = 6;

Action BidToAction(Bid bid);
Bid ActionToBid(Action action);
bool IsBidAction(Action action);  // one of the four kActionBid* values

bool IsBiddingAction(Action action);  // any auction action (bid, pass or hold)
std::string BiddingActionToString(Action action);

class BiddingState {
 public:
  BiddingState() = default;
  explicit BiddingState(const std::vector<PlayerBidInfo>& info);

  Player CurrentPlayer() const { return current_player_; }
  std::vector<Action> LegalActions() const;
  void ApplyAction(Action action);

  bool IsFinished() const { return finished_; }
  // True once finished with nobody having bid (the hand is thrown in).
  bool PassedOut() const { return finished_ && declarer_ == kInvalidPlayer; }
  Player Declarer() const { return declarer_; }
  Bid WinningBid() const { return winning_bid_; }
  // How many players made a positive bid (bid or held) during the auction; used
  // by the §5.7 trull announcement promises.
  int NumBidders() const {
    int n = 0;
    for (const absl::optional<Bid>& b : player_bid_) {
      if (b.has_value()) ++n;
    }
    return n;
  }

  // Conventional outcomes (see file comment). CueBidder() is kInvalidPlayer if
  // no cue bid was made; Yielded() is true for a yielded game;
  // ObligatoryCalledCard() is what the declarer must call (kNone = free / XX).
  Player CueBidder() const { return cue_bidder_; }
  CalledCard CuedCard() const { return cued_card_; }
  bool Yielded() const { return yielded_; }
  CalledCard ObligatoryCalledCard() const { return obligatory_called_card_; }
  // True (once finished) if an *accepted* invite obliges pagátultimó (C
  // §5.2.2): a cue bid by a player whose only honour is the pagát, where the
  // cue-bidder did not win the auction (so the winner is forced to call them
  // in). If the cue-bidder wins outright the invite is not accepted and there
  // is no obligation. Decided from the bidding-time hand, so a high honour
  // later drawn from the talon does not lift it.
  bool PagatUltiObligation() const { return pagat_ulti_obligation_; }

  std::string ToString() const;

 private:
  int NumPlayers() const { return static_cast<int>(info_.size()); }
  int NumActive() const;  // players who have not passed
  bool HasBid(Player p) const { return player_bid_[p].has_value(); }
  bool CanHold(Player p) const;
  bool IsFourthAfterThreePasses() const;
  bool IsYieldPosition() const;
  // If `bid` by player `p` would be a cue bid (given the current auction
  // state), returns the promised card, otherwise kNone.
  CalledCard CueForBid(Player p, Bid bid) const;

  void ApplyBiddingAction(Action action);
  void ApplySoleRaise(Action action);
  void AdvanceOrFinish();
  void Finish(Player winner, Bid bid);

  std::vector<PlayerBidInfo> info_;
  Player current_player_ = kInvalidPlayer;
  std::vector<bool> passed_;
  // The highest bid each player has made (nullopt = they have not bid).
  std::vector<absl::optional<Bid>> player_bid_;
  // The standing bid (nullopt = no bid has been made yet).
  absl::optional<Bid> current_bid_;
  Player current_bidder_ = kInvalidPlayer;
  bool last_was_hold_ = false;
  bool awaiting_sole_raise_ = false;

  Player cue_bidder_ = kInvalidPlayer;
  CalledCard cued_card_ = CalledCard::kNone;
  bool yielded_ = false;

  bool finished_ = false;
  Player declarer_ = kInvalidPlayer;
  Bid winning_bid_ = Bid::kThree;
  CalledCard obligatory_called_card_ = CalledCard::kNone;
  bool pagat_ulti_obligation_ = false;
  std::vector<std::pair<Player, Action>> calls_;  // public log, for ToString()
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_BIDDING_H_
