#include "open_spiel/games/hungarian_tarokk/hungarian_tarokk.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/game_parameters.h"
#include "open_spiel/games/hungarian_tarokk/scoring.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_globals.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace hungarian_tarokk {
namespace {

// Facts about the game.
const GameType kGameType{/*short_name=*/"hungarian_tarokk",
                         /*long_name=*/"Hungarian Tarokk",
                         GameType::Dynamics::kSequential,
                         GameType::ChanceMode::kExplicitStochastic,
                         GameType::Information::kImperfectInformation,
                         GameType::Utility::kZeroSum,
                         GameType::RewardModel::kTerminal,
                         /*max_num_players=*/kNumPlayers,
                         /*min_num_players=*/kNumPlayers,
                         /*provides_information_state_string=*/false,
                         /*provides_information_state_tensor=*/false,
                         /*provides_observation_string=*/true,
                         /*provides_observation_tensor=*/true,
                         /*parameter_specification=*/{}};

std::shared_ptr<const Game> Factory(const GameParameters& params) {
  return std::shared_ptr<const Game>(new HungarianTarokkGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

std::string PhaseToString(Phase phase) {
  switch (phase) {
    case Phase::kDealing:
      return "Dealing";
    case Phase::kBidding:
      return "Bidding";
    case Phase::kTalonExchange:
      return "TalonExchange";
    case Phase::kAnnouncements:
      return "Announcements";
    case Phase::kPlaying:
      return "Playing";
    case Phase::kFinished:
      return "Finished";
  }
  SpielFatalError("Unknown phase.");
}

std::string PlayerLabel(Player p) { return p < 0 ? "-" : absl::StrCat("P", p); }

// The bidding action ids live immediately after the card action ids.
static_assert(kBiddingActionBase == kNumCards,
              "Bidding action ids must start right after the card action ids.");

}  // namespace

HungarianTarokkState::HungarianTarokkState(std::shared_ptr<const Game> game)
    : State(game),
      deck_(NewSortedDeck()),
      players_cards_(kNumPlayers),
      discarded_(kNumPlayers) {
  collected_points_.fill(0);
}

const std::vector<std::vector<Card>>& HungarianTarokkState::CurrentHands()
    const {
  return phase_ == Phase::kTalonExchange ? talon_exchange_.Hands()
                                         : players_cards_;
}

const std::vector<std::vector<Card>>& HungarianTarokkState::CurrentDiscards()
    const {
  return phase_ == Phase::kTalonExchange ? talon_exchange_.Discards()
                                         : discarded_;
}

const std::vector<Card>& HungarianTarokkState::CurrentTalon() const {
  return phase_ == Phase::kTalonExchange ? talon_exchange_.Talon() : talon_;
}

Player HungarianTarokkState::CurrentPlayer() const {
  if (phase_ == Phase::kFinished) return kTerminalPlayerId;
  if (phase_ == Phase::kDealing) return kChancePlayerId;
  if (phase_ == Phase::kBidding) return bidding_.CurrentPlayer();
  if (phase_ == Phase::kTalonExchange) return talon_exchange_.CurrentPlayer();
  if (phase_ == Phase::kAnnouncements) return announcements_.CurrentPlayer();
  return current_player_;  // play
}

bool HungarianTarokkState::IsTerminal() const {
  return phase_ == Phase::kFinished;
}

ActionsAndProbs HungarianTarokkState::ChanceOutcomes() const {
  if (phase_ == Phase::kTalonExchange) return talon_exchange_.ChanceOutcomes();
  SPIEL_CHECK_TRUE(phase_ == Phase::kDealing);
  ActionsAndProbs outcomes;
  outcomes.reserve(deck_.size());
  const double p = 1.0 / static_cast<double>(deck_.size());
  for (Card card : deck_) outcomes.push_back({CardToAction(card), p});
  return outcomes;
}

std::vector<Action> HungarianTarokkState::LegalActions() const {
  if (phase_ == Phase::kFinished) return {};
  // chance: undealt cards
  if (phase_ == Phase::kDealing) return CardsToActions(deck_);
  if (phase_ == Phase::kBidding) return bidding_.LegalActions();
  if (phase_ == Phase::kTalonExchange) return talon_exchange_.LegalActions();
  if (phase_ == Phase::kAnnouncements) return announcements_.LegalActions();
  return LegalPlayActions();
}

std::vector<Action> HungarianTarokkState::LegalPlayActions() const {
  const std::vector<Card>& hand = players_cards_[current_player_];
  std::vector<Card> legal;
  if (trick_cards_.empty()) {
    // Leading: any card may be played.
    legal = hand;
  } else {
    int led_suit = CardSuit(trick_cards_.front());
    // Must follow the led suit if possible.
    for (Card c : hand) {
      if (CardSuit(c) == led_suit) legal.push_back(c);
    }
    if (legal.empty()) {
      // Otherwise must play a tarokk if holding one.
      for (Card c : hand) {
        if (IsTarokk(c)) legal.push_back(c);
      }
    }
    // Otherwise (no led suit, no tarokk) anything goes.
    if (legal.empty()) legal = hand;
  }
  std::sort(legal.begin(), legal.end());
  return CardsToActions(legal);
}

void HungarianTarokkState::DoApplyAction(Action action_id) {
  if (phase_ == Phase::kDealing) {
    const Card card = CardFromAction(action_id);
    auto it = std::find(deck_.begin(), deck_.end(), card);
    SPIEL_CHECK_TRUE(it != deck_.end());
    deck_.erase(it);
    Player target = cards_dealt_ % kNumPlayers;
    players_cards_[target].push_back(card);
    ++cards_dealt_;
    if (cards_dealt_ == kCardsDealtToPlayers) {
      // The remaining six cards form the talon.
      talon_ = deck_;
      deck_.clear();
      for (std::vector<Card>& hand : players_cards_) {
        std::sort(hand.begin(), hand.end());
      }
      // Start the auction.
      std::vector<PlayerBidInfo> info(kNumPlayers);
      for (int p = 0; p < kNumPlayers; ++p) {
        const std::vector<Card>& hand = players_cards_[p];
        info[p].has_honour = HandHasHonour(hand);
        info[p].has_high_honour = HandHasHighHonour(hand);
        info[p].has_xx = HandHasCard(hand, kCardXX);
        info[p].has_xix = HandHasCard(hand, kCardXIX);
        info[p].has_xviii = HandHasCard(hand, kCardXVIII);
      }
      bidding_ = BiddingState(info);
      phase_ = Phase::kBidding;
      // history_ is appended with the current (final dealing) action only after
      // DoApplyAction returns, so the first bidding action lands at size() + 1.
      bidding_history_start_ = history_.size() + 1;
    }
    return;
  }

  if (phase_ == Phase::kBidding) {
    bidding_.ApplyAction(action_id);
    if (bidding_.IsFinished()) {
      if (bidding_.PassedOut()) {
        // Nobody bid: the hand is thrown in with no score.
        phase_ = Phase::kFinished;
      } else {
        declarer_ = bidding_.Declarer();
        winning_bid_ = bidding_.WinningBid();
        // Hand the hands and the talon over to the exchange sub-machine.
        talon_exchange_ = TalonExchangeState(
            std::move(players_cards_), std::move(talon_), declarer_,
            winning_bid_, bidding_.CueBidder(), bidding_.CuedCard());
        phase_ = Phase::kTalonExchange;
        // As above: the current (final bidding) action is appended after this
        // returns, so the one-past-the-last index is size() + 1.
        bidding_history_end_ = history_.size() + 1;
      }
    }
    return;
  }

  if (phase_ == Phase::kTalonExchange) {
    talon_exchange_.ApplyAction(action_id);
    if (talon_exchange_.IsFinished()) {
      players_cards_ = talon_exchange_.Hands();
      discarded_ = talon_exchange_.Discards();
      if (talon_exchange_.Annulled()) {
        annulled_ = true;
        phase_ = Phase::kFinished;  // thrown-in hand, no score
      } else {
        // Placeholder scoring: each player keeps the points of their own skart.
        // (With partnerships the defenders' skart is pooled for the opposing
        // side; that is left for later.)
        for (int p = 0; p < kNumPlayers; ++p) {
          for (Card c : discarded_[p]) collected_points_[p] += CardPoints(c);
        }
        const Player ulti_player = bidding_.PagatUltiObligation()
                                       ? bidding_.CueBidder()
                                       : kInvalidPlayer;
        announcements_ =
            AnnouncementState(players_cards_, declarer_, winning_bid_,
                              bidding_.ObligatoryCalledCard(), ulti_player,
                              bidding_.NumBidders(), discarded_);
        phase_ = Phase::kAnnouncements;
      }
    }
    return;
  }

  if (phase_ == Phase::kAnnouncements) {
    announcements_.ApplyAction(action_id);
    if (announcements_.IsFinished()) {
      called_card_ = announcements_.CalledCardTarokk();
      partner_ = announcements_.Partner();
      StartPlaying();
    }
    return;
  }

  // Playing phase.
  const Card card = CardFromAction(action_id);
  std::vector<Card>& hand = players_cards_[current_player_];
  auto it = std::find(hand.begin(), hand.end(), card);
  SPIEL_CHECK_TRUE(it != hand.end());
  hand.erase(it);
  trick_cards_.push_back(card);
  if (trick_cards_.size() < kNumPlayers) {
    current_player_ = (current_player_ + 1) % kNumPlayers;
  } else {
    ResolveTrick();
  }
}

void HungarianTarokkState::ResolveTrick() {
  int led_suit = CardSuit(trick_cards_.front());
  int best_index = 0;
  for (int i = 1; i < static_cast<int>(trick_cards_.size()); ++i) {
    if (CardBeats(trick_cards_[best_index], trick_cards_[i], led_suit)) {
      best_index = i;
    }
  }
  Player winner = (trick_leader_ + best_index) % kNumPlayers;
  for (Card c : trick_cards_) collected_points_[winner] += CardPoints(c);
  completed_tricks_.push_back(trick_cards_);
  trick_winners_.push_back(winner);
  trick_cards_.clear();
  ++tricks_played_;
  if (tricks_played_ == kNumTricks) {
    phase_ = Phase::kFinished;
    current_player_ = kTerminalPlayerId;
  } else {
    trick_leader_ = winner;
    current_player_ = winner;
  }
}

void HungarianTarokkState::StartPlaying() {
  phase_ = Phase::kPlaying;
  trick_leader_ = 0;  // the forehand (player 0) leads the first trick
  current_player_ = 0;
}

DealScore HungarianTarokkState::BuildDealScore() const {
  const auto side_of = [&](Player p) {
    return (p == declarer_ || p == partner_) ? Side::kDeclarers
                                             : Side::kDefenders;
  };

  // Reconstruct, for every played card, which trick it fell in, who played it,
  // and which side captured it. The forehand (P0) leads the first trick; each
  // trick's winner leads the next.
  std::array<Side, kNumCards> captor;
  std::array<Player, kNumCards> player_of;
  std::array<int, kNumCards> trick_of;
  int declarer_trick_points = 0;
  int declarer_tricks = 0;
  Player leader = 0;
  for (size_t t = 0; t < completed_tricks_.size(); ++t) {
    const std::vector<Card>& tc = completed_tricks_[t];
    const Player winner = trick_winners_[t];
    const Side ws = side_of(winner);
    int points = 0;
    for (int i = 0; i < kNumPlayers; ++i) {
      const Card c = tc[i];
      captor[c.index] = ws;
      player_of[c.index] = (leader + i) % kNumPlayers;
      trick_of[c.index] = t;
      points += CardPoints(c);
    }
    if (ws == Side::kDeclarers) {
      declarer_trick_points += points;
      ++declarer_tricks;
    }
    leader = winner;
  }

  DealScore d;
  d.declarer = declarer_;
  d.partner = partner_;
  d.base_value = BidGameValue(winning_bid_);
  d.declarer_tricks = declarer_tricks;

  // §7.1: the declarer's side adds only its own skart; the other three players'
  // discards all count for the opponents.
  int declarer_discard_points = 0;
  for (Card c : discarded_[declarer_]) declarer_discard_points += CardPoints(c);
  d.declarer_card_points = declarer_trick_points + declarer_discard_points;

  // Trull (all three honours) and four kings, by capturing side.
  for (Side s : {Side::kDeclarers, Side::kDefenders}) {
    const int i = static_cast<int>(s);
    d.trull_made[i] = captor[kCardSkiz.index] == s &&
                      captor[kCardXXI.index] == s &&
                      captor[kCardPagat.index] == s;
    d.four_kings_made[i] = captor[MakeKing(kHearts).index] == s &&
                           captor[MakeKing(kDiamonds).index] == s &&
                           captor[MakeKing(kClubs).index] == s &&
                           captor[MakeKing(kSpades).index] == s;
  }

  // Pagátultimó: the pagát's side, and whether it won or lost the last trick.
  const Player pagat_player = player_of[kCardPagat.index];
  d.pagat_side = side_of(pagat_player);
  if (trick_of[kCardPagat.index] == kNumTricks - 1) {
    d.pagat_last_trick =
        (trick_winners_[kNumTricks - 1] == pagat_player) ? 1 : -1;
  } else {
    d.pagat_last_trick = 0;
  }

  // XXI-catch: the Skíz took the opponents' XXI in the same trick (§5.2 -- not
  // a catch when the two fall together from partners).
  const Player xxi_player = player_of[kCardXXI.index];
  const Player skiz_player = player_of[kCardSkiz.index];
  if (trick_of[kCardXXI.index] == trick_of[kCardSkiz.index] &&
      side_of(xxi_player) != side_of(skiz_player)) {
    d.xxi_caught = true;
    d.xxi_catcher_side = side_of(skiz_player);
  }

  // Announcements: per-(bonus, side) announcement and kontra level, the game
  // kontra, and each player's tarokk-count declaration.
  for (int b = 0; b < kNumBonuses; ++b) {
    const Bonus bonus = static_cast<Bonus>(b);
    for (Side s : {Side::kDeclarers, Side::kDefenders}) {
      const int i = static_cast<int>(s);
      d.announced[b][i] = announcements_.BonusAnnounced(bonus, s);
      d.bonus_kontra[b][i] = announcements_.BonusKontraLevel(bonus, s);
    }
  }
  d.game_kontra = announcements_.GameKontraLevel();
  for (Player p = 0; p < kNumPlayers; ++p) {
    d.declared_tarokks[p] = announcements_.DeclaredTarokks(p);
  }
  return d;
}

std::vector<double> HungarianTarokkState::Returns() const {
  // Zero returns until a deal is actually scored: while play is unfinished, or
  // for a passed-out / annulled hand that never reached the play (no score).
  if (phase_ != Phase::kFinished || tricks_played_ != kNumTricks) {
    return std::vector<double>(kNumPlayers, 0.0);
  }
  const std::array<double, kNumPlayers> scores = ScoreDeal(BuildDealScore());
  return std::vector<double>(scores.begin(), scores.end());
}

std::string HungarianTarokkState::PublicLogString() const {
  std::vector<std::string> tricks;
  tricks.reserve(completed_tricks_.size());
  for (const std::vector<Card>& t : completed_tricks_) {
    tricks.push_back(CardsToString(t));
  }
  return absl::StrJoin(tricks, " / ");
}

std::string HungarianTarokkState::ActionToString(Player player,
                                                 Action action_id) const {
  if (player == kChancePlayerId) {
    const char* verb = phase_ == Phase::kTalonExchange ? "Draw " : "Deal ";
    return absl::StrCat(verb, CardToString(CardFromAction(action_id)));
  }
  if (IsBiddingAction(action_id)) return BiddingActionToString(action_id);
  if (action_id == kActionAnnul) return "Annul";
  if (action_id == kActionDeclineAnnul) return "DeclineAnnul";
  if (IsDiscardAction(action_id)) {
    return absl::StrCat("Discard ",
                        CardToString(CardForDiscardAction(action_id)));
  }
  if (action_id >= kCallActionBase && action_id <= kLastAnnounceAction) {
    return AnnouncementActionToString(action_id);
  }
  return CardToString(CardFromAction(action_id));  // playing a card
}

std::string HungarianTarokkState::ToString() const {
  const std::vector<std::vector<Card>>& hands = CurrentHands();
  const std::vector<std::vector<Card>>& discards = CurrentDiscards();
  std::string str = absl::StrCat("Phase: ", PhaseToString(phase_), "\n");
  for (int p = 0; p < kNumPlayers; ++p) {
    absl::StrAppend(&str, "P", p, " hand: ", CardsToString(hands[p]));
    if (!discards[p].empty()) {
      absl::StrAppend(&str, " skart: ", CardsToString(discards[p]));
    }
    absl::StrAppend(&str, "\n");
  }
  if (!CurrentTalon().empty()) {
    absl::StrAppend(&str, "Talon: ", CardsToString(CurrentTalon()), "\n");
  }
  if (phase_ != Phase::kDealing) {
    absl::StrAppend(&str, "Bidding: ", bidding_.ToString(), "\n");
  }
  if (phase_ == Phase::kTalonExchange) {
    absl::StrAppend(&str, "Talon exchange: ", talon_exchange_.ToString(), "\n");
  }
  std::string ann = announcements_.ToString();
  if (!ann.empty()) absl::StrAppend(&str, "Announcements: ", ann, "\n");
  if (partner_ != kInvalidPlayer) {
    absl::StrAppend(&str, "Partner: P", partner_, "\n");
  }
  absl::StrAppend(&str, "Tricks: ", PublicLogString(), "\n");
  if (!trick_cards_.empty()) {
    absl::StrAppend(&str, "Current trick: ", CardsToString(trick_cards_), "\n");
  }
  absl::StrAppend(&str, "Points: ", absl::StrJoin(collected_points_, ","),
                  "\n");
  return str;
}

std::string HungarianTarokkState::InformationStateString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, kNumPlayers);
  // The player's private knowledge: their own current hand and skart plus
  // everything that is public (the bidding and the cards played).
  std::string str = absl::StrCat(
      "P", player, " hand: ", CardsToString(CurrentHands()[player]),
      " | skart: ", CardsToString(CurrentDiscards()[player]),
      " | bidding: ", bidding_.ToString(),
      " | announcements: ", announcements_.ToString(),
      " | history: ", PublicLogString());
  if (!trick_cards_.empty()) {
    absl::StrAppend(&str, " | trick: ", CardsToString(trick_cards_));
  }
  return str;
}

std::string HungarianTarokkState::ObservationString(Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, kNumPlayers);
  // The announcement sub-state is only meaningful once that phase is reached
  // (and never for a passed-out / annulled hand).
  const bool ann_ready =
      (phase_ == Phase::kAnnouncements || phase_ == Phase::kPlaying ||
       phase_ == Phase::kFinished) &&
      declarer_ != kInvalidPlayer && !annulled_;

  // Phase, whose turn it is, and the observer's own hand.
  std::string str = absl::StrCat("P", player, " ", PhaseToString(phase_),
                                 " turn:", PlayerLabel(CurrentPlayer()));
  absl::StrAppend(&str, " | hand: ", CardsToString(CurrentHands()[player]));

  // Auction: declarer, the winning (or, while bidding, the standing) bid, the
  // obligatory call, then the seven bid-slots -- 3, 2, hold@2, 1, hold@1, solo,
  // hold@solo -- replayed from the history (so a skipped bidder still sees the
  // whole auction here). Each slot shows the player who reached it, or "-".
  absl::StrAppend(&str, " | declarer:", PlayerLabel(declarer_));
  if (declarer_ != kInvalidPlayer) {
    absl::StrAppend(&str, " bid:", BidToString(winning_bid_));
  } else if (phase_ == Phase::kBidding && bidding_.StandingBid().has_value()) {
    absl::StrAppend(&str, " bid:", BidToString(*bidding_.StandingBid()));
  }
  if (bidding_.ObligatoryCalledCard() != CalledCard::kNone) {
    absl::StrAppend(
        &str, " oblig:", CalledCardToString(bidding_.ObligatoryCalledCard()));
  }
  constexpr int kNumBidSlots = 2 * kNumBids - 1;
  std::array<Player, kNumBidSlots> bid_players;
  bid_players.fill(kInvalidPlayer);
  auto bid_slot = [](Bid b) {
    const int i = static_cast<int>(b);
    return i > 0 ? i * 2 - 1 : 0;
  };
  Bid highest_bid = kWeakestBid;  // a hold can never precede the first bid
  for (auto it = BiddingHistoryBegin(); it != BiddingHistoryEnd(); ++it) {
    if (it->action == kActionPass) continue;  // dropped out, not a slot
    if (it->action == kActionHold) {
      bid_players[bid_slot(highest_bid) + 1] = it->player;
    } else {
      highest_bid = ActionToBid(it->action);
      bid_players[bid_slot(highest_bid)] = it->player;
    }
  }
  std::vector<std::string> slots;
  slots.reserve(kNumBidSlots);
  for (Player p : bid_players) slots.push_back(PlayerLabel(p));
  absl::StrAppend(&str, " bidders:[", absl::StrJoin(slots, ","), "]");

  if (ann_ready) {
    // Called tarokk, the publicly-known sides, the tarokk-count declarations,
    // the hivatalból kontra, then the bonus announcements (with their kontra
    // levels) and the game kontra.
    const Card called = announcements_.CalledCardTarokk();
    if (called != kInvalidCard) {
      absl::StrAppend(&str, " | called:", CardToString(called));
    }
    std::vector<std::string> sides;
    for (Player q = 0; q < kNumPlayers; ++q) {
      absl::optional<Side> sd = announcements_.PublicSide(q);
      if (q == player &&
          called != kInvalidCard) {  // observer knows its own side
        sd = (q == declarer_ || q == announcements_.Partner())
                 ? Side::kDeclarers
                 : Side::kDefenders;
      }
      sides.push_back(!sd.has_value() ? "?"
                                      : (*sd == Side::kDeclarers ? "D" : "d"));
    }
    absl::StrAppend(&str, " sides:[", absl::StrJoin(sides, ","), "]");
    std::vector<std::string> decl;
    bool any_decl = false;
    for (Player q = 0; q < kNumPlayers; ++q) {
      const int d = announcements_.DeclaredTarokks(q);
      decl.push_back(d ? absl::StrCat(d) : "-");
      if (d) any_decl = true;
    }
    if (any_decl) {
      absl::StrAppend(&str, " tarokks:[", absl::StrJoin(decl, ","), "]");
    }
    if (announcements_.HivatalbolKontraPlayer() != kInvalidPlayer) {
      absl::StrAppend(&str, " hiv:P", announcements_.HivatalbolKontraPlayer());
    }
    std::vector<std::string> anns;
    for (int b = 0; b < kNumBonuses; ++b) {
      for (int s = 0; s < 2; ++s) {
        const Bonus bonus = static_cast<Bonus>(b);
        const Side side = static_cast<Side>(s);
        if (!announcements_.BonusAnnounced(bonus, side)) continue;
        const int k = announcements_.BonusKontraLevel(bonus, side);
        anns.push_back(absl::StrCat(BonusToString(bonus), s == 0 ? "(D" : "(d",
                                    k ? absl::StrCat("+k", k) : "", ")"));
      }
    }
    if (!anns.empty()) {
      absl::StrAppend(&str, " ann:[", absl::StrJoin(anns, ","), "]");
    }
    if (announcements_.GameKontraLevel() > 0) {
      absl::StrAppend(&str, " gk:", announcements_.GameKontraLevel());
    }
  }

  // §6.3 discarded-tarokk counts and §6.4 the declarer's face-up skart tarokks
  // (shown only until the first trick completes, matching the tensor).
  const std::vector<std::vector<Card>>& discards = CurrentDiscards();
  bool any_discard = false;
  std::vector<std::string> counts;
  for (Player q = 0; q < kNumPlayers; ++q) {
    int n = 0;
    for (Card c : discards[q]) {
      if (IsTarokk(c)) ++n;
    }
    counts.push_back(absl::StrCat(n));
    if (!discards[q].empty()) any_discard = true;
  }
  if (any_discard) {
    absl::StrAppend(&str, " | disc:[", absl::StrJoin(counts, ","), "]");
  }
  const bool skart_shown =
      declarer_ != kInvalidPlayer &&
      (phase_ == Phase::kAnnouncements ||
       (phase_ == Phase::kPlaying && completed_tricks_.empty()));
  if (skart_shown) {
    std::vector<Card> st;
    for (Card c : discards[declarer_]) {
      if (IsTarokk(c)) st.push_back(c);
    }
    if (!st.empty()) absl::StrAppend(&str, " shown:", CardsToString(st));
  }

  // Play: the trick in progress (with its leader), then the most-recent
  // completed trick (with the player who led it). The last trick's winner is
  // the current trick's leader, so it is not repeated -- exactly as the tensor.
  if (!trick_cards_.empty()) {
    absl::StrAppend(&str, " | trick(P", trick_leader_,
                    "): ", CardsToString(trick_cards_));
  }
  if (!completed_tricks_.empty()) {
    const Player last_leader = trick_winners_.size() > 1
                                   ? trick_winners_[trick_winners_.size() - 2]
                                   : 0;
    absl::StrAppend(&str, " | last(P", last_leader,
                    "): ", CardsToString(completed_tricks_.back()));
  }
  return str;
}

void HungarianTarokkState::ObservationTensor(Player player,
                                             absl::Span<float> values) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, kNumPlayers);
  SPIEL_CHECK_EQ(static_cast<int>(values.size()), kObservationTensorSize);
  std::fill(values.begin(), values.end(), 0.0f);
  constexpr int N = kNumPlayers;
  // Players are encoded relative to the observer (0 = self); -1 marks "none".
  auto rel = [&](Player q) { return q < 0 ? -1 : (q - player + N) % N; };

  int pos = 0;
  auto onehot = [&](int idx, int size) {
    if (idx >= 0 && idx < size) values[pos + idx] = 1.0f;
    pos += size;
  };
  auto player_none = [&](Player q) {  // player one-hot with a trailing "none"
    const int r = rel(q);
    onehot(r < 0 ? N : r, N + 1);
  };
  auto player_plain = [&](Player q) { onehot(rel(q), N); };  // all-zero if none
  auto scalar = [&](float v) { values[pos++] = v; };
  auto cards_multihot = [&](const std::vector<Card>& cards, bool tarokk_only) {
    for (Card c : cards) {
      if (tarokk_only && !IsTarokk(c)) continue;
      if (c.index >= 0 && c.index < kNumCards) values[pos + c.index] = 1.0f;
    }
    pos += kNumCards;
  };

  const bool ann_ready =
      (phase_ == Phase::kAnnouncements || phase_ == Phase::kPlaying ||
       phase_ == Phase::kFinished) &&
      declarer_ != kInvalidPlayer && !annulled_;

  // Phase, whose turn it is, and the observer's own hand.
  onehot(static_cast<int>(phase_), 6);
  player_plain(CurrentPlayer());
  cards_multihot(CurrentHands()[player], /*tarokk_only=*/false);

  // Auction: declarer, the standing/winning bid, the obligatory call, and each
  // player's highest bid (all persist so a skipped bidder sees them later).
  player_none(declarer_);
  absl::optional<Bid> shown_bid;
  if (declarer_ != kInvalidPlayer) {
    shown_bid = winning_bid_;
  } else if (phase_ == Phase::kBidding) {
    shown_bid = bidding_.StandingBid();
  }
  onehot(shown_bid.has_value() ? static_cast<int>(*shown_bid) : kNumBids,
         kNumBids + 1);
  onehot(static_cast<int>(bidding_.ObligatoryCalledCard()), 4);

  // The auction as seven bid-slots -- 3, 2, hold@2, 1, hold@1, solo, hold@solo
  // -- each holding the (relative) player who reached it, or "none". Replaying
  // the bidding history reconstructs it, so a player skipped during the auction
  // still sees the whole thing at its next turn. (3 has no hold-slot: a new
  // bidder must raise, it cannot hold.)
  constexpr int kNumBidSlots = 2 * kNumBids - 1;
  std::array<int, kNumBidSlots> bid_players;
  bid_players.fill(-1);  // -1 = nobody bid this slot
  auto bid_slot = [&](Bid b) {
    const int i = static_cast<int>(b);
    return i > 0 ? i * 2 - 1 : 0;
  };
  Bid highest_bid = kWeakestBid;  // a hold can never precede the first bid
  for (auto bid_it = BiddingHistoryBegin(); bid_it != BiddingHistoryEnd();
       ++bid_it) {
    if (bid_it->action == kActionPass) continue;  // dropped out, not a slot
    int slot;
    if (bid_it->action == kActionHold) {
      slot = bid_slot(highest_bid) + 1;
    } else {
      highest_bid = ActionToBid(bid_it->action);
      slot = bid_slot(highest_bid);
    }
    bid_players[slot] = rel(bid_it->player);
  }
  for (int slot = 0; slot < kNumBidSlots; ++slot) {
    scalar(bid_players[slot] < 0 ? -1.0f
                                 : static_cast<float>(bid_players[slot]));
  }

  // Announcements (all guarded: the sub-state's arrays are only valid once its
  // phase is reached). Called tarokk, sides, tarokk counts, hivatalból kontra.
  const Card called =
      ann_ready ? announcements_.CalledCardTarokk() : kInvalidCard;
  const Player partner = ann_ready ? announcements_.Partner() : kInvalidPlayer;
  onehot(called == kInvalidCard ? kNumTarokks : called.index, kNumTarokks + 1);
  for (int r = 0; r < N; ++r) {
    const Player q = (player + r) % N;
    int side = 2;  // unknown
    if (declarer_ != kInvalidPlayer && q == declarer_) side = 0;
    if (ann_ready) {
      absl::optional<Side> sd = announcements_.PublicSide(q);
      if (sd.has_value()) side = static_cast<int>(*sd);
      if (q == player && called != kInvalidCard) {
        side = (q == declarer_ || q == partner) ? 0 : 1;
      }
    }
    onehot(side, 3);
  }
  for (int r = 0; r < N; ++r) {
    const int d =
        ann_ready ? announcements_.DeclaredTarokks((player + r) % N) : 0;
    onehot(d == 9 ? 2 : (d == 8 ? 1 : 0), 3);
  }
  player_none(ann_ready ? announcements_.HivatalbolKontraPlayer()
                        : kInvalidPlayer);

  // Bonus announcements, their kontra levels, and the game kontra.
  for (int b = 0; b < kNumBonuses; ++b) {
    for (int s = 0; s < 2; ++s) {
      scalar(ann_ready && announcements_.BonusAnnounced(static_cast<Bonus>(b),
                                                        static_cast<Side>(s))
                 ? 1.0f
                 : 0.0f);
    }
  }
  for (int b = 0; b < kNumBonuses; ++b) {
    for (int s = 0; s < 2; ++s) {
      const int k = ann_ready ? announcements_.BonusKontraLevel(
                                    static_cast<Bonus>(b), static_cast<Side>(s))
                              : 0;
      scalar(static_cast<float>(k));
    }
  }
  scalar(ann_ready ? static_cast<float>(announcements_.GameKontraLevel())
                   : 0.0f);

  // §6.3 discarded-tarokk counts, §6.4 the declarer's shown skart, the points.
  const std::vector<std::vector<Card>>& discards = CurrentDiscards();
  for (int r = 0; r < N; ++r) {
    int n = 0;
    for (Card c : discards[(player + r) % N]) {
      if (IsTarokk(c)) ++n;
    }
    scalar(static_cast<float>(n));
  }
  const bool skart_shown =
      declarer_ != kInvalidPlayer &&
      (phase_ == Phase::kAnnouncements ||
       (phase_ == Phase::kPlaying && completed_tricks_.empty()));
  if (skart_shown) {
    cards_multihot(discards[declarer_], /*tarokk_only=*/true);
  } else {
    pos += kNumCards;
  }

  auto add_trick = [&](const std::vector<Card>& trick, Player leader) {
    std::array<Card, kNumPlayers> trick_cards;
    trick_cards.fill(kInvalidCard);
    for (int i = 0; i < static_cast<int>(trick.size()); ++i) {
      trick_cards[(leader + i) % N] = trick[i];
    }
    for (int r = 0; r < N; ++r) {
      const Card c = trick_cards[(player + r) % N];
      onehot(c == kInvalidCard ? kNumCards : c.index, kNumCards + 1);
    }
  };

  // Play: the current trick (which card each player played), its leader, then
  // the most-recently completed trick's cards and its winner.
  add_trick(trick_cards_, trick_leader_);
  SPIEL_CHECK_GE(trick_leader_, 0);
  player_plain(trick_leader_);
  if (!completed_tricks_.empty()) {
    Player last_leader;
    if (trick_winners_.size() > 1)
      last_leader = trick_winners_[trick_winners_.size() - 2];
    else
      last_leader = 0;  // the forehand led the first trick
    add_trick(completed_tricks_.back(), last_leader);
  } else {
    pos += (kNumCards + 1) * kNumPlayers;
  }

  SPIEL_CHECK_EQ(pos, kObservationTensorSize);
}

std::unique_ptr<ObservationStruct> HungarianTarokkState::ToObservationStruct(
    Player player) const {
  SPIEL_CHECK_GE(player, 0);
  SPIEL_CHECK_LT(player, kNumPlayers);
  constexpr int N = kNumPlayers;
  auto obs = std::make_unique<HungarianTarokkObservationStruct>();
  obs->observing_player = player;

  const bool ann_ready =
      (phase_ == Phase::kAnnouncements || phase_ == Phase::kPlaying ||
       phase_ == Phase::kFinished) &&
      declarer_ != kInvalidPlayer && !annulled_;
  const auto seat = [](Player p) { return p < 0 ? -1 : static_cast<int>(p); };

  // Phase, whose turn it is, and the observer's own hand.
  obs->phase = static_cast<int>(phase_);
  obs->current_player = seat(CurrentPlayer());
  for (Card c : CurrentHands()[player]) obs->hand.push_back(c.index);

  // Auction: declarer, the winning (or standing) bid, the obligatory call, then
  // the seven bid-slots replayed from the history (see ObservationTensor).
  obs->declarer = seat(declarer_);
  if (declarer_ != kInvalidPlayer) {
    obs->bid = static_cast<int>(winning_bid_);
  } else if (phase_ == Phase::kBidding && bidding_.StandingBid().has_value()) {
    obs->bid = static_cast<int>(*bidding_.StandingBid());
  } else {
    obs->bid = -1;
  }
  obs->obligatory_call = static_cast<int>(bidding_.ObligatoryCalledCard());
  constexpr int kNumBidSlots = 2 * kNumBids - 1;
  obs->bid_slots.assign(kNumBidSlots, -1);
  auto bid_slot = [](Bid b) {
    const int i = static_cast<int>(b);
    return i > 0 ? i * 2 - 1 : 0;
  };
  Bid highest_bid = kWeakestBid;  // a hold can never precede the first bid
  for (auto it = BiddingHistoryBegin(); it != BiddingHistoryEnd(); ++it) {
    obs->bidding_history.push_back(
        HungarianTarokkCall{seat(it->player), static_cast<int>(it->action)});
    if (it->action == kActionPass) continue;  // dropped out, not a slot
    if (it->action == kActionHold) {
      obs->bid_slots[bid_slot(highest_bid) + 1] = it->player;
    } else {
      highest_bid = ActionToBid(it->action);
      obs->bid_slots[bid_slot(highest_bid)] = it->player;
    }
  }

  // Announcements (defaults until that phase is reached). The called tarokk,
  // the publicly-known sides (with the observer's own side revealed if it holds
  // the called card), the tarokk-count declarations, the hivatalból kontra,
  // then the bonus announcements with their kontra levels and the game kontra.
  const Card called =
      ann_ready ? announcements_.CalledCardTarokk() : kInvalidCard;
  const Player partner = ann_ready ? announcements_.Partner() : kInvalidPlayer;
  obs->called_tarokk = called == kInvalidCard ? -1 : called.index;
  for (Player q = 0; q < N; ++q) {
    int side = -1;  // unknown
    if (declarer_ != kInvalidPlayer && q == declarer_) {
      side = static_cast<int>(Side::kDeclarers);
    }
    if (ann_ready) {
      absl::optional<Side> sd = announcements_.PublicSide(q);
      if (sd.has_value()) side = static_cast<int>(*sd);
      if (q == player && called != kInvalidCard) {
        side = static_cast<int>((q == declarer_ || q == partner)
                                    ? Side::kDeclarers
                                    : Side::kDefenders);
      }
    }
    obs->sides.push_back(side);
  }
  for (Player q = 0; q < N; ++q) {
    obs->declared_tarokks.push_back(
        ann_ready ? announcements_.DeclaredTarokks(q) : 0);
  }
  obs->hivatalbol_kontra =
      ann_ready ? seat(announcements_.HivatalbolKontraPlayer()) : -1;
  if (ann_ready) {
    for (int b = 0; b < kNumBonuses; ++b) {
      for (int s = 0; s < 2; ++s) {
        const Bonus bonus = static_cast<Bonus>(b);
        const Side side = static_cast<Side>(s);
        if (!announcements_.BonusAnnounced(bonus, side)) continue;
        obs->bonus_announcements.push_back(HungarianTarokkBonusAnnouncement{
            b, s, announcements_.BonusKontraLevel(bonus, side)});
      }
    }
  }
  obs->game_kontra = ann_ready ? announcements_.GameKontraLevel() : 0;
  for (const std::pair<Player, Action>& call : announcements_.Log()) {
    obs->announcement_history.push_back(
        HungarianTarokkCall{seat(call.first), static_cast<int>(call.second)});
  }

  // §6.3 discarded-tarokk counts and §6.4 the declarer's face-up skart tarokks.
  const std::vector<std::vector<Card>>& discards = CurrentDiscards();
  for (Player q = 0; q < N; ++q) {
    int n = 0;
    for (Card c : discards[q]) {
      if (IsTarokk(c)) ++n;
    }
    obs->discard_tarokk_counts.push_back(n);
  }
  const bool skart_shown =
      declarer_ != kInvalidPlayer &&
      (phase_ == Phase::kAnnouncements ||
       (phase_ == Phase::kPlaying && completed_tricks_.empty()));
  if (skart_shown) {
    for (Card c : discards[declarer_]) {
      if (IsTarokk(c)) obs->declarer_shown_tarokks.push_back(c.index);
    }
  }

  // Trick play: each player's card (by absolute seat) in the trick in progress
  // and in the last completed one; the last trick's winner is the current
  // trick's leader, so it is not repeated -- exactly as the tensor.
  obs->current_trick.assign(N, -1);
  for (int i = 0; i < static_cast<int>(trick_cards_.size()); ++i) {
    obs->current_trick[(trick_leader_ + i) % N] = trick_cards_[i].index;
  }
  obs->current_trick_leader = trick_cards_.empty() ? -1 : seat(trick_leader_);
  obs->last_trick.assign(N, -1);
  if (!completed_tricks_.empty()) {
    const Player last_leader = trick_winners_.size() > 1
                                   ? trick_winners_[trick_winners_.size() - 2]
                                   : 0;
    const std::vector<Card>& t = completed_tricks_.back();
    for (int i = 0; i < static_cast<int>(t.size()); ++i) {
      obs->last_trick[(last_leader + i) % N] = t[i].index;
    }
  }

  // The full trick-by-trick replay: trick 0 is always led by the forehand
  // (player 0), and each later trick is led by the previous one's winner.
  Player leader = 0;
  for (size_t t = 0; t < completed_tricks_.size(); ++t) {
    const std::vector<Card>& cards = completed_tricks_[t];
    HungarianTarokkTrick trick;
    trick.leader = seat(leader);
    trick.cards.assign(N, -1);
    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
      trick.cards[(leader + i) % N] = cards[i].index;
    }
    trick.winner = seat(trick_winners_[t]);
    obs->trick_history.push_back(std::move(trick));
    leader = trick_winners_[t];
  }
  return obs;
}

std::unique_ptr<State> HungarianTarokkState::Clone() const {
  return std::unique_ptr<State>(new HungarianTarokkState(*this));
}

HungarianTarokkGame::HungarianTarokkGame(const GameParameters& params)
    : Game(kGameType, params) {}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
