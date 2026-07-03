#include "open_spiel/games/hungarian_tarokk/announcements.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarokk {

std::string BonusToString(Bonus bonus) {
  switch (bonus) {
    case Bonus::kTrull:
      return "Trull";
    case Bonus::kFourKings:
      return "FourKings";
    case Bonus::kPagatUlti:
      return "PagatUlti";
    case Bonus::kXxiCatch:
      return "XXICatch";
    case Bonus::kDoubleGame:
      return "DoubleGame";
    case Bonus::kVolat:
      return "Volat";
  }
  SpielFatalError("Unknown bonus.");
}

std::string AnnouncementActionToString(Action action) {
  if (IsCallAction(action)) {
    return absl::StrCat("Call ", CardToString(TarokkForCallAction(action)));
  }
  if (IsAnnounceBonusAction(action)) {
    return absl::StrCat(
        "Announce ",
        BonusToString(static_cast<Bonus>(action - kAnnounceBonusBase)));
  }
  if (IsKontraAction(action)) {
    int item = action - kKontraActionBase;
    if (item == kGameKontraItem) return "Kontra Game";
    Bonus bonus = static_cast<Bonus>((item - 1) / 2);
    Side side = static_cast<Side>((item - 1) % 2);
    return absl::StrCat(
        "Kontra ", BonusToString(bonus),
        side == Side::kDeclarers ? " (declarers')" : " (defenders')");
  }
  if (action == kActionDeclareEight) return "Declare8Tarokks";
  if (action == kActionDeclareNine) return "Declare9Tarokks";
  if (action == kActionAnnouncePass) return "Pass";
  SpielFatalError(absl::StrCat("Not an announcement action: ", action));
}

AnnouncementState::AnnouncementState(std::vector<std::vector<Card>> hands,
                                     Player declarer, Bid bid,
                                     CalledCard obligatory,
                                     Player pagat_ulti_player,
                                     std::vector<std::vector<Card>> discards)
    : hands_(std::move(hands)),
      discards_(std::move(discards)),
      declarer_(declarer),
      bid_(bid),
      obligatory_(obligatory),
      pagat_ulti_player_(pagat_ulti_player),
      current_player_(declarer) {
  if (discards_.empty()) discards_.resize(kNumPlayers);  // no skart given
  for (std::array<bool, 2>& sides : bonus_announced_) sides.fill(false);
  kontra_level_.fill(0);
  pagat_ulti_committed_.fill(false);
  declared_tarokks_.fill(0);
  public_side_.fill(absl::nullopt);
  public_side_[declarer_] = Side::kDeclarers;  // the declarer's side is public
}

bool AnnouncementState::NonDeclarerDiscardedTarokk() const {
  for (Player q = 0; q < kNumPlayers; ++q) {
    if (q == declarer_) continue;
    for (Card c : discards_[q]) {
      if (IsTarokk(c)) return true;
    }
  }
  return false;
}

Side AnnouncementState::SideOf(Player p) const {
  return (p == declarer_ || p == partner_) ? Side::kDeclarers
                                           : Side::kDefenders;
}

std::vector<Action> AnnouncementState::LegalCalls() const {
  // A cue bid or yield forces the called card.
  if (obligatory_ == CalledCard::kXIX) return {CallActionForTarokk(kCardXIX)};
  if (obligatory_ == CalledCard::kXVIII) {
    return {CallActionForTarokk(kCardXVIII)};
  }
  if (obligatory_ == CalledCard::kXX) return {CallActionForTarokk(kCardXX)};

  const std::vector<Card>& dh = hands_[declarer_];
  if (!HandHasCard(dh, kCardXX)) {
    // The declarer lacks the XX, so calls it. (The XX may never be discarded, so
    // its holder is always a current hand -- a real partner.)
    return {CallActionForTarokk(kCardXX)};
  }

  // The declarer holds the XX.
  if (NonDeclarerDiscardedTarokk()) {
    // §6.5: with a tarokk in the skart the declarer may call any tarokk except an
    // honour, one it holds itself (you call yourself only with the XX), or one it
    // discarded -- so it may call a tarokk a defender laid away (§4.3). It may
    // still call its own XX to play alone.
    std::vector<Action> calls;
    for (int t = 0; t < kNumTarokks; ++t) {
      const Card c{t};
      if (c == kCardPagat || c == kCardXXI || c == kCardSkiz) continue;
      if (HandHasCard(dh, c)) continue;
      if (HandHasCard(discards_[declarer_], c)) continue;
      calls.push_back(CallActionForTarokk(c));
    }
    calls.push_back(CallActionForTarokk(kCardXX));  // own XX -> play alone
    std::sort(calls.begin(), calls.end());
    return calls;
  }

  // Otherwise: call the highest tarokk below the XX it does not hold, or its own
  // XX to play alone.
  Card highest_below = kInvalidCard;
  for (int t = kCardXX.index - 1; t >= 0; --t) {
    if (!HandHasCard(dh, Card{t})) {
      highest_below = Card{t};
      break;
    }
  }
  std::vector<Action> calls = {CallActionForTarokk(highest_below),
                               CallActionForTarokk(kCardXX)};
  return calls;
}

bool AnnouncementState::BonusAnnounceable(int bonus, Player p) const {
  const int side = static_cast<int>(SideOf(p));
  if (bonus_announced_[bonus][side]) return false;
  // once volát has been announced, no trull, four kings or double game.
  const int volat = static_cast<int>(Bonus::kVolat);
  if (bonus_announced_[volat][side] &&
      (bonus == static_cast<int>(Bonus::kTrull) ||
       bonus == static_cast<int>(Bonus::kFourKings) ||
       bonus == static_cast<int>(Bonus::kDoubleGame))) {
    return false;
  }
  return true;
}

absl::optional<Side> AnnouncementState::KontraRaiserSide(int item) const {
  Side owner;
  if (item == kGameKontraItem) {
    // The declarer's side owns the game (it took the contract).
    owner = Side::kDeclarers;
  } else {
    const int bonus = static_cast<int>(BonusForKontraItem(item));
    const Side claim_side = SideForKontraItem(item);
    if (!bonus_announced_[bonus][static_cast<int>(claim_side)]) {
      return absl::nullopt;  // no such announcement, nothing to kontra
    }
    owner = claim_side;
  }
  int level = kontra_level_[item];
  // The opponents kontra (even -> odd); the owners rekontra (odd -> even).
  return (level % 2 == 0) ? Opponent(owner) : owner;
}

bool AnnouncementState::HasPendingObligation(Player p) const {
  // The cue-bidder whose only honour is the pagát must announce pagátultimó
  // (C §5.2.2): until its side has, the player may not end its turn.
  const int ulti = static_cast<int>(Bonus::kPagatUlti);
  if (p == pagat_ulti_player_ &&
      !bonus_announced_[ulti][static_cast<int>(SideOf(p))]) {
    return true;
  }
  // Having announced or kontra'd a pagátultimó, a player must declare its 8/9
  // tarokks if it holds them before passing.
  if (pagat_ulti_committed_[p] && declared_tarokks_[p] == 0 &&
      CountTarokks(hands_[p]) >= 8) {
    return true;
  }
  return false;
}

bool AnnouncementState::CanAnnounceForOwnSide(Player p) const {
  // If p's side is already public it may announce for it directly; otherwise the
  // §5.5 convention attributes the announcement to the last speaker's side (the
  // declarer's if none), so it only lands on p's own side when the two match.
  if (public_side_[p].has_value()) return true;
  return ConventionDefaultSide() == SideOf(p);
}

void AnnouncementState::RevealSide(Player p) {
  public_side_[p] = SideOf(p);
  last_speaker_side_ = SideOf(p);
  DeducePublicSides();
}

void AnnouncementState::DeducePublicSides() {
  bool partner_identified = false;
  int known_defenders = 0;
  int unknown = 0;
  Player last_unknown = kInvalidPlayer;

  for (Player p = 0; p < kNumPlayers; ++p) {
    if (p == declarer_) continue;
    if (!public_side_[p].has_value()) {
      ++unknown;
      last_unknown = p;
    } else if (*public_side_[p] == Side::kDeclarers) {
      partner_identified = true;  // a non-declarer on the declarer's side
    } else {
      ++known_defenders;
    }
  }

  // Once the partner is identified, every other non-declarer is a defender.
  if (partner_identified && unknown > 0) {
    for (Player p = 0; p < kNumPlayers; ++p) {
      if (p != declarer_ && !public_side_[p].has_value()) {
        public_side_[p] = Side::kDefenders;
      }
    }
  }
  // If a partner is known to exist and the other two are known defenders, the
  // last unknown must be that partner. (Not so after a bare XX call, where the
  // declarer might instead be playing alone.)
  if (partner_known_to_exist_ && known_defenders == 2 && unknown == 1) {
    public_side_[last_unknown] = Side::kDeclarers;
  }
}

std::vector<Action> AnnouncementState::LegalActions() const {
  SPIEL_CHECK_FALSE(finished_);
  Player p = current_player_;
  // The declarer must call a partner before anything else.
  if (p == declarer_ && called_card_ == kInvalidCard) return LegalCalls();

  std::vector<Action> legal;
  // Pass ends the turn -- forbidden while a mandatory declaration is still owed
  if (!HasPendingObligation(p)) legal.push_back(kActionAnnouncePass);
  // Bonus and tarokk-count announcements are side-carrying: a player whose side
  // is not yet public may only make one that the §5.5 convention attributes to
  // its own side (otherwise it must first reveal itself with a kontra).
  const bool can_announce = CanAnnounceForOwnSide(p);
  for (int b = 0; b < kNumBonuses; ++b) {
    if (can_announce && BonusAnnounceable(b, p))
      legal.push_back(AnnounceBonusAction(static_cast<Bonus>(b)));
  }
  for (int i = 0; i < kNumKontraItems; ++i) {
    // No side-deduction requirements for kontras, because you have
    // to be against the side that announced the bonus.
    if (kontra_level_[i] < kMaxKontra && KontraRaiserSide(i) == SideOf(p)) {
      legal.push_back(
          KontraClaimAction(BonusForKontraItem(i), SideForKontraItem(i)));
    }
  }
  // A player holding 8/9 tarokks may declare their tarokk count (also side-
  // carrying, so subject to the same reveal rule).
  if (can_announce && declared_tarokks_[p] == 0) {
    const int n = CountTarokks(hands_[p]);
    if (n == 9) legal.push_back(kActionDeclareNine);
    else if (n == 8) legal.push_back(kActionDeclareEight);
  }
  std::sort(legal.begin(), legal.end());
  return legal;
}

void AnnouncementState::ApplyAction(Action action) {
  SPIEL_CHECK_FALSE(finished_);
  Player p = current_player_;
  log_.push_back({p, action});

  if (IsCallAction(action)) {
    called_card_ = TarokkForCallAction(action);
    partner_ = kInvalidPlayer;
    hivatalbol_kontra_player_ = kInvalidPlayer;
    for (Player q = 0; q < kNumPlayers; ++q) {
      if (q != declarer_ && HandHasCard(hands_[q], called_card_)) {
        partner_ = q;  // (none holds it -> own XX, or a discarded tarokk)
        break;
      }
    }
    if (partner_ == kInvalidPlayer) {
      // The called card is in no hand: either the declarer called its own XX to
      // play alone, or it called a tarokk a defender discarded (§4.3). In the
      // latter case the discarder kontras the game "by office" (hivatalból
      // kontra) -- applied automatically here, not as a player action.
      for (Player q = 0; q < kNumPlayers; ++q) {
        if (q != declarer_ && HandHasCard(discards_[q], called_card_)) {
          hivatalbol_kontra_player_ = q;
          kontra_level_[kGameKontraItem] += 1;  // the automatic by-office kontra
          break;
        }
      }
    }
    // §5.5 public side-deduction from the call.
    if (hivatalbol_kontra_player_ != kInvalidPlayer) {
      // A discarded tarokk was called: the declarer plays alone and the table
      // learns it (the by-office kontra is public), so every non-declarer is a
      // known defender.
      partner_known_to_exist_ = false;
      for (Player q = 0; q < kNumPlayers; ++q) {
        if (q != declarer_) public_side_[q] = Side::kDefenders;
      }
      last_speaker_side_ = Side::kDefenders;
    } else if (obligatory_ != CalledCard::kNone) {
      // A forced call (an accepted cue bid, or a yield) names the publicly-known
      // inviter as the partner, so the whole table's sides become public.
      partner_known_to_exist_ = true;
      if (partner_ != kInvalidPlayer) public_side_[partner_] = Side::kDeclarers;
    } else if (called_card_ == kCardXX) {
      // A bare XX call may instead be the declarer calling itself to play alone,
      // so the public does not yet know a partner exists.
      partner_known_to_exist_ = false;
    } else {
      // Calling any other tarokk requires a partner (you may call yourself only
      // with the XX), though its identity stays hidden for now.
      partner_known_to_exist_ = true;
    }
    DeducePublicSides();
    spoke_this_turn_ = true;
    return;  // still the declarer's turn
  }

  if (IsAnnounceBonusAction(action)) {
    const int bonus = action - kAnnounceBonusBase;
    bonus_announced_[bonus][static_cast<int>(SideOf(p))] = true;
    if (bonus == static_cast<int>(Bonus::kPagatUlti)) {
      pagat_ulti_committed_[p] = true;  // must now declare 8/9 if held
    }
    RevealSide(p);
    spoke_this_turn_ = true;
    return;
  }

  if (IsKontraAction(action)) {
    const int item = action - kKontraActionBase;
    kontra_level_[item] += 1;
    if (item != kGameKontraItem &&
        BonusForKontraItem(item) == Bonus::kPagatUlti) {
      pagat_ulti_committed_[p] = true;  // kontra-ing ulti also forces 8/9
    }
    RevealSide(p);
    spoke_this_turn_ = true;
    return;
  }

  if (IsTarokkDeclareAction(action)) {
    declared_tarokks_[p] = (action == kActionDeclareNine) ? 9 : 8;
    RevealSide(p);
    spoke_this_turn_ = true;
    return;
  }

  SPIEL_CHECK_EQ(action, kActionAnnouncePass);
  EndTurn();
}

void AnnouncementState::EndTurn() {
  if (spoke_this_turn_) {
    consecutive_passes_ = 0;
  } else {
    ++consecutive_passes_;
  }
  spoke_this_turn_ = false;
  if (consecutive_passes_ >= kNumPlayers - 1) {
    finished_ = true;
    return;
  }
  current_player_ = (current_player_ + 1) % kNumPlayers;
}

std::string AnnouncementState::ToString() const {
  std::vector<std::string> parts;
  parts.reserve(log_.size());
  for (const std::pair<Player, Action>& call : log_) {
    parts.push_back(absl::StrCat("P", call.first, ":",
                                 AnnouncementActionToString(call.second)));
  }
  std::string str = absl::StrJoin(parts, " ");
  // The hivatalból kontra is automatic (not a logged action); surface it here so
  // it shows up in observations (§4.3).
  if (hivatalbol_kontra_player_ != kInvalidPlayer) {
    absl::StrAppend(&str, str.empty() ? "" : " ", "[hivatalbol kontra by P",
                    hivatalbol_kontra_player_, "]");
  }
  return str;
}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
