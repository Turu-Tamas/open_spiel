#include "open_spiel/games/hungarian_tarokk/hungarian_tarokk.h"

#include <algorithm>
#include <array>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_join.h"
#include "open_spiel/game_parameters.h"
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
                         /*provides_information_state_string=*/true,
                         /*provides_information_state_tensor=*/false,
                         /*provides_observation_string=*/true,
                         /*provides_observation_tensor=*/false,
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
        announcements_ =
            AnnouncementState(players_cards_, declarer_, winning_bid_,
                              bidding_.ObligatoryCalledCard());
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

std::vector<double> HungarianTarokkState::Returns() const {
  if (phase_ != Phase::kFinished) {
    return std::vector<double>(kNumPlayers, 0.0);
  }
  int total =
      std::accumulate(collected_points_.begin(), collected_points_.end(), 0);
  double mean = static_cast<double>(total) / kNumPlayers;
  std::vector<double> returns(kNumPlayers);
  for (int p = 0; p < kNumPlayers; ++p) {
    returns[p] = static_cast<double>(collected_points_[p]) - mean;
  }
  return returns;
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
  std::string str = absl::StrCat(
      "P", player, " hand: ", CardsToString(CurrentHands()[player]),
      " | skart: ", CardsToString(CurrentDiscards()[player]),
      " | bidding: ", bidding_.ToString());
  if (!trick_cards_.empty()) {
    absl::StrAppend(&str, " | trick: ", CardsToString(trick_cards_));
  }
  absl::StrAppend(&str, " | points: ", absl::StrJoin(collected_points_, ","));
  return str;
}

std::unique_ptr<State> HungarianTarokkState::Clone() const {
  return std::unique_ptr<State>(new HungarianTarokkState(*this));
}

HungarianTarokkGame::HungarianTarokkGame(const GameParameters& params)
    : Game(kGameType, params) {}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
