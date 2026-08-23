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
//   call partner tarokk t          kCallActionBase + t        (92..113)
//   announce bonus b                kAnnounceBonusBase + b     (114..119)
//   kontra the game / bonus b,      kKontraActionBase + i      (120..147)
//   at level l
//     (i = group*4 + l; group 0 is the game, group 1+bonus is that bonus; l
//     is 0=kontra, 1=rekontra, 2=szubkontra, 3=hirskontra -- these are
//     distinct calls in the real game, C §5.3, so there is never ambiguity
//     between e.g. a kontra and a rekontra. A kontra action still names no
//     side: which side's claim it hits is resolved from the level and the
//     kontra-ing player's own side -- see AnnouncementState::KontraTargetItem.)
//   declare 8 / 9 tarokks            kActionDeclareEight/Nine  (148, 149)
//   pass                             kActionAnnouncePass       (150)
inline constexpr Action kCallActionBase = kActionDeclineAnnul + 1;
inline constexpr Action kAnnounceBonusBase = kCallActionBase + kNumTarokks;
inline constexpr Action kKontraActionBase = kAnnounceBonusBase + kNumBonuses;
// Both sides may announce the same bonus independently (§5.2) and each such
// announcement carries its own kontra chain (§5.3), so internally the state
// tracks kontra levels for the game plus one per (bonus, side) pair.
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
// A kontra action: which group (the game, or a bonus) crossed with which
// level (kontra, rekontra, szub-, hirskontra) -- but never which side, C
// §5.3.
inline constexpr int kNumKontraGroups = 1 + kNumBonuses;
inline constexpr int kNumKontraLevels = 4;  // kontra, rekontra, szub-, hirskontra
inline constexpr int kNumKontraActions = kNumKontraGroups * kNumKontraLevels;
inline constexpr Action KontraGameAction(int level) {
  return kKontraActionBase + level;
}
inline constexpr Action KontraBonusAction(Bonus b, int level) {
  return kKontraActionBase + (1 + static_cast<int>(b)) * kNumKontraLevels +
         level;
}
inline constexpr Action kActionKontraGame = KontraGameAction(0);
inline constexpr Action kActionRekontraGame = KontraGameAction(1);
inline constexpr Action kActionSzubkontraGame = KontraGameAction(2);
inline constexpr Action kActionHirskontraGame = KontraGameAction(3);
// 8/9 tarokk declarations (tarokkszám, §5.4), then the turn-ending pass.
inline constexpr Action kActionDeclareEight =
    kKontraActionBase + kNumKontraActions;
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
  return a >= kKontraActionBase && a < kKontraActionBase + kNumKontraActions;
}
std::string AnnouncementActionToString(Action action);

inline Action AnnounceBonusAction(Bonus b) {
  return kAnnounceBonusBase + static_cast<int>(b);
}
inline bool IsTarokkDeclareAction(Action a) {
  return a == kActionDeclareEight || a == kActionDeclareNine;
}

class AnnouncementState {
 public:
  AnnouncementState() = default;
  // `hands` are the players' (post-skart) nine-card hands and `discards` their
  // skart (used to call a discarded tarokk, §4.3); `obligatory` is the card the
  // auction forces the declarer to call (kNone = free choice).
  // `pagat_ulti_player` (kInvalidPlayer if none) is the cue-bidder who must
  // announce pagátultimó (C §5.2.2), decided during the auction. `num_bidders`
  // is how many players made a positive bid in the auction, used by the §5.7
  // trull announcement promises.
  AnnouncementState(std::vector<std::vector<Card>> hands, Player declarer,
                    Bid bid, CalledCard obligatory,
                    Player pagat_ulti_player = kInvalidPlayer,
                    int num_bidders = 0,
                    std::vector<std::vector<Card>> discards = {});

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
  // The side a player is publicly known to be on, or nullopt if it cannot yet
  // be deduced from the call and the announcements so far (§5.5).
  absl::optional<Side> PublicSide(Player p) const { return public_side_[p]; }
  // The player who, having discarded the called tarokk, kontra'd the game "by
  // office" (hivatalból kontra, §4.3); kInvalidPlayer if no discarded tarokk
  // was called. This kontra is automatic, not a player action.
  Player HivatalbolKontraPlayer() const { return hivatalbol_kontra_player_; }

  bool BonusAnnounced(Bonus b, Side s) const {
    return bonus_announced_[static_cast<int>(b)][static_cast<int>(s)];
  }
  int BonusKontraLevel(Bonus b, Side s) const {
    return kontra_level_[KontraItemForBonus(b, s)];
  }
  int GameKontraLevel() const { return kontra_level_[kGameKontraItem]; }

  std::string ToString() const;  // public log (does not reveal the partner)

 private:
  static constexpr int kMaxKontra = kNumKontraLevels;

  Side SideOf(Player p) const;
  std::vector<Action> LegalCalls() const;
  // Whether any non-declarer put a tarokk in its skart, enabling the §6.5 free
  // call (declarer holding the XX may then call almost any tarokk).
  bool NonDeclarerDiscardedTarokk() const;
  bool BonusAnnounceable(int bonus, Player p) const;
  // Whether p's hand satisfies the honour promise that announcing trull carries
  bool TrullPromiseMet(Player p) const;
  // The side allowed to raise item `i`'s kontra next, or nullopt if none.
  absl::optional<Side> KontraRaiserSide(int item) const;
  // Which kontra item a bare kontra action at `level` (no side named) in
  // `group` (0 = the game, 1+bonus = that bonus) by `p` resolves to, or
  // nullopt if p may not make that call right now. Kontra/szubkontra (even
  // levels) are always the claim owner's opponent raising; rekontra/
  // hirskontra (odd levels) are always the owner raising back -- so the
  // level, together with p's own (true) side, always names exactly one
  // owner (C §5.3: kontra, rekontra, szub- and hirskontra are distinct
  // calls, so there is no ambiguity between different levels). If only one
  // of the (up to two) claims to `group` is at `level`, that one is the
  // unambiguous answer regardless of p (already deducible from public
  // bonus/kontra state). If both are -- both sides independently announced
  // the bonus (§5.2) and each got raised in turn -- which one this call
  // hits is only deducible from p's own side, so exactly like a side-
  // carrying announcement (§5.5) it is legal only once that's safe: p's
  // side is already public, or the convention default already matches it
  // (see CanAnnounceForOwnSide).
  absl::optional<int> KontraTargetItem(int group, int level, Player p) const;
  // Whether player p still owes a mandatory declaration this turn (the obliged
  // pagátultimó, or an 8/9 tarokk count after committing to a pagátultimó).
  bool HasPendingObligation(Player p) const;

  // §5.5 public side-deduction. The side an as-yet-unrevealed announcer is
  // assumed to take: that of the most recent speaker, or the declarer's side if
  // nobody has spoken.
  Side ConventionDefaultSide() const {
    return last_speaker_side_.value_or(Side::kDeclarers);
  }
  // Whether p may make a side-carrying call (a bonus or tarokk-count
  // announcement, or a kontra/rekontra that could equally hit either side's
  // claim, see KontraTargetItem) that is correctly attributed to its own
  // side without first revealing it: its side is already public, or the
  // convention default already matches it.
  bool CanAnnounceForOwnSide(Player p) const;
  // Record that p revealed its side by speaking, then re-run the deduction.
  void RevealSide(Player p);
  // Fill in sides that are now deducible by elimination (e.g. once the partner
  // is identified the rest are defenders).
  void DeducePublicSides();
  void EndTurn();

  std::vector<std::vector<Card>> hands_;
  std::vector<std::vector<Card>> discards_;  // each player's skart (§4.3)
  Player declarer_ = kInvalidPlayer;
  Bid bid_;
  CalledCard obligatory_;
  // The cue-bidder obliged to announce pagátultimó (kInvalidPlayer if none).
  Player pagat_ulti_player_ = kInvalidPlayer;
  int num_bidders_ = 0;  // players who made a positive bid (for §5.7)

  Player current_player_ = kInvalidPlayer;
  Card called_card_ = kInvalidCard;
  Player partner_ = kInvalidPlayer;
  // The player who kontra'd the game by office after the declarer called the
  // tarokk it had discarded (hivatalból kontra, §4.3); kInvalidPlayer if none.
  Player hivatalbol_kontra_player_ = kInvalidPlayer;
  // Which sides have announced each bonus ([bonus][side]); both sides may
  // announce the same bonus independently (§5.2).
  std::array<std::array<bool, 2>, kNumBonuses> bonus_announced_;
  std::array<int, kNumKontraItems> kontra_level_;  // 0 .. kMaxKontra
  // Players who announced or kontra'd a pagátultimó: they must declare their
  // tarokk count if they hold 8/9 (C §4.1.3).
  std::array<bool, kNumPlayers> pagat_ulti_committed_;
  std::array<int, kNumPlayers> declared_tarokks_;  // 0, 8 or 9 (tarokkszám)
  // How many turns each player has already completed in the announcement phase;
  // 0 means the player is in its first round (relevant to the §5.7 promises).
  std::array<int, kNumPlayers> turns_taken_;
  // §5.5 public side-deduction: each player's publicly-known side (nullopt =
  // not yet deducible), whether a partner is publicly known to exist at all
  // (false after a bare XX call, which may be a lone declarer), and the side of
  // the most recent speaker (the convention base).
  std::array<absl::optional<Side>, kNumPlayers> public_side_;
  bool partner_known_to_exist_ = false;
  absl::optional<Side> last_speaker_side_;
  bool spoke_this_turn_ = false;
  int consecutive_passes_ = 0;
  bool finished_ = false;
  std::vector<std::pair<Player, Action>> log_;  // public, for ToString()
};

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_ANNOUNCEMENTS_H_
