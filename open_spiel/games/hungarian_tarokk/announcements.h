#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_ANNOUNCEMENTS_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_ANNOUNCEMENTS_H_

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/games/hungarian_tarokk/bidding.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/games/hungarian_tarokk/talon.h"
#include "open_spiel/spiel.h"

// Simplifications for now (to be refined later, see rules.md):
//   * sides are taken from the *actual* partnership (the engine knows who holds
//     the called card) rather than from the public side-deduction of §5.5;
//   * the tulétroá honour promises (§5.7) are not modelled, and announced
//     bonuses / tarokk declarations do not yet affect scoring;
//   * calling a discarded tarokk (§4.3) is not handled.

namespace open_spiel {
namespace hungarian_tarokk {

// The two sides of the table: the declarer + the called partner against the
// other two players.
enum class Side { kDeclarers = 0, kDefenders = 1 };
inline Side Opponent(Side side) {
  return side == Side::kDeclarers ? Side::kDefenders : Side::kDeclarers;
}

enum class Bonus {
  kTrull,
  kFourKings,
  kPagatUlti,
  kXxiCatch,
  kDoubleGame,
  kVolat
};
inline constexpr int kNumBonuses = 6;
std::string BonusToString(Bonus bonus);

// Announcement action ids, in their own disjoint range after the talon actions
// (..91):
//   call partner tarokk t        kCallActionBase + t       (92..113)
//   announce bonus b             kAnnounceBonusBase + b     (114..119)
//   kontra item i                kKontraActionBase + i      (120..132)
//     (i = 0 is the game; i = 1 + 2*bonus + side is a (bonus, side) claim)
//   declare 8 / 9 tarokks        kActionDeclareEight/Nine   (133, 134)
//   pass                         kActionAnnouncePass        (135)
inline constexpr Action kCallActionBase = kActionDeclineAnnul + 1;
inline constexpr Action kAnnounceBonusBase = kCallActionBase + kNumTarokks;
inline constexpr Action kKontraActionBase = kAnnounceBonusBase + kNumBonuses;
// Both sides may announce the same bonus independently (§5.2) and each such
// announcement carries its own kontra chain (§5.3), so the kontra items are the
// game plus one per (bonus, side) pair.
inline constexpr int kGameKontraItem = 0;
inline constexpr int kNumKontraItems = 1 + 2 * kNumBonuses;
inline constexpr int KontraItemForBonus(Bonus bonus, Side side) {
  return 1 + 2 * static_cast<int>(bonus) + static_cast<int>(side);
}
inline constexpr Bonus BonusForKontraItem(int item) {
  return static_cast<Bonus>((item - 1) /
                            2);  // (item = 0 is the game, not a bonus)
}
inline constexpr Side SideForKontraItem(int item) {
  return static_cast<Side>((item - 1) % 2);
}
// 8/9 tarokk declarations (tarokkszám, §5.4), then the turn-ending pass.
inline constexpr Action kActionDeclareEight =
    kKontraActionBase + kNumKontraItems;
inline constexpr Action kActionDeclareNine = kActionDeclareEight + 1;
inline constexpr Action kActionAnnouncePass = kActionDeclareNine + 1;
inline constexpr Action kLastAnnounceAction = kActionAnnouncePass;

inline bool IsCallAction(Action a) {
  return a >= kCallActionBase && a < kCallActionBase + kNumTarokks;
}
inline Action CallActionForTarokk(Card tarokk) {
  return kCallActionBase + tarokk.index;
}
inline Card TarokkForCallAction(Action a) {
  return Card{static_cast<int>(a - kCallActionBase)};
}
inline bool IsAnnounceBonusAction(Action a) {
  return a >= kAnnounceBonusBase && a < kAnnounceBonusBase + kNumBonuses;
}
inline bool IsKontraAction(Action a) {
  return a >= kKontraActionBase && a < kKontraActionBase + kNumKontraItems;
}
std::string AnnouncementActionToString(Action action);

inline Action AnnounceBonusAction(Bonus b) {
  return kAnnounceBonusBase + static_cast<int>(b);
}
// Kontra the bonus `b` as claimed by `side`
inline Action KontraClaimAction(Bonus b, Side side) {
  return kKontraActionBase + KontraItemForBonus(b, side);
}
inline bool IsTarokkDeclareAction(Action a) {
  return a == kActionDeclareEight || a == kActionDeclareNine;
}

class AnnouncementState {
 public:
  AnnouncementState() = default;
  // `hands` are the players' (post-skart) nine-card hands; `obligatory` is the
  // card the auction forces the declarer to call (kNone = free choice).
  // `pagat_ulti_player` (kInvalidPlayer if none) is the cue-bidder who must
  // announce pagátultimó (C §5.2.2), decided during the auction.
  AnnouncementState(std::vector<std::vector<Card>> hands, Player declarer,
                    Bid bid, CalledCard obligatory,
                    Player pagat_ulti_player = kInvalidPlayer);

  Player CurrentPlayer() const { return current_player_; }
  std::vector<Action> LegalActions() const;
  void ApplyAction(Action action);
  bool IsFinished() const { return finished_; }

  Card CalledCardTarokk() const { return called_card_; }
  // The holder of the called tarokk, or kInvalidPlayer if the declarer called
  // their own XX (playing alone).
  Player Partner() const { return partner_; }
  // The tarokk count (0, 8 or 9) a player declared (tarokkszám, §5.4).
  int DeclaredTarokks(Player p) const { return declared_tarokks_[p]; }

  std::string ToString() const;  // public log (does not reveal the partner)

 private:
  static constexpr int kMaxKontra = 4;  // kontra, rekontra, szub-, hirskontra

  Side SideOf(Player p) const;
  std::vector<Action> LegalCalls() const;
  bool BonusAnnounceable(int bonus, Player p) const;
  // The side allowed to raise item `i`'s kontra next, or nullopt if none.
  absl::optional<Side> KontraRaiserSide(int item) const;
  // Whether player p still owes a mandatory declaration this turn (the obliged
  // pagátultimó, or an 8/9 tarokk count after committing to a pagátultimó).
  bool HasPendingObligation(Player p) const;
  void EndTurn();

  std::vector<std::vector<Card>> hands_;
  Player declarer_ = kInvalidPlayer;
  Bid bid_;
  CalledCard obligatory_;
  // The cue-bidder obliged to announce pagátultimó (kInvalidPlayer if none).
  Player pagat_ulti_player_ = kInvalidPlayer;

  Player current_player_ = kInvalidPlayer;
  Card called_card_ = kInvalidCard;
  Player partner_ = kInvalidPlayer;
  // Which sides have announced each bonus ([bonus][side]); both sides may
  // announce the same bonus independently (§5.2).
  std::array<std::array<bool, 2>, kNumBonuses> bonus_announced_;
  std::array<int, kNumKontraItems> kontra_level_;  // 0 .. kMaxKontra
  // Players who announced or kontra'd a pagátultimó: they must declare their
  // tarokk count if they hold 8/9 (C §4.1.3).
  std::array<bool, kNumPlayers> pagat_ulti_committed_;
  std::array<int, kNumPlayers> declared_tarokks_;  // 0, 8 or 9 (tarokkszám)
  bool spoke_this_turn_ = false;
  int consecutive_passes_ = 0;
  bool finished_ = false;
  std::vector<std::pair<Player, Action>> log_;  // public, for ToString()
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_ANNOUNCEMENTS_H_
