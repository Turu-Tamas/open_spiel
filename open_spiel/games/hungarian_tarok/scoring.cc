#include "open_spiel/games/hungarian_tarok/scoring.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"

namespace open_spiel {
namespace hungarian_tarok {

bool ScoringSummary::declarer_announced(AnnouncementType type) const {
  return declarer_side.announced_for(type);
}

bool ScoringSummary::opponents_announced(AnnouncementType type) const {
  return opponents_side.announced_for(type);
}

bool ScoringSummary::side_announced(Side side, AnnouncementType type) const {
  return side == Side::kDeclarer ? declarer_announced(type)
                                 : opponents_announced(type);
}

bool ScoringSummary::game_contrad() const {
  return declarer_side.contra_level_for(AnnouncementType::kGame) > 0;
}

namespace {

Side CardWinnerSide(const CommonState& game_data, Card card) {
  return game_data.deck_[card] == PlayerWonCardsLocation(game_data.declarer_) ||
                 (game_data.partner_ &&
                  game_data.deck_[card] ==
                      PlayerWonCardsLocation(*game_data.partner_))
             ? Side::kDeclarer
             : Side::kOpponents;
}

std::optional<Side> CardSetWinnerSide(const CommonState& game_data,
                                      const std::vector<Card>& cards) {
  Side first_side = CardWinnerSide(game_data, cards[0]);
  for (Card card : cards) {
    Side side = CardWinnerSide(game_data, card);
    if (side != first_side) {
      return std::nullopt;
    }
  }
  return first_side;
}

std::optional<Side> VolatWinnerSide(const CommonState& game_data) {
  Side first_side = game_data.player_sides_[game_data.trick_winners_.front()];
  return std::all_of(
             game_data.trick_winners_.begin(), game_data.trick_winners_.end(),
             [&](Player p) { return game_data.player_sides_[p] == first_side; })
             ? std::optional<Side>(first_side)
             : std::nullopt;
}

int DeclarerCardPoints(const CommonState& game_data) {
  int points = 0;
  for (Card card = 0; card < kDeckSize; ++card) {
    const CardLocation location = game_data.deck_[card];
    if (IsWonCards(location)) {
      const Player owner = WonCardsLocationPlayer(location);
      if (game_data.player_sides_[owner] == Side::kDeclarer) {
        points += CardPointValue(card);
      }
    }
  }
  return points;
}

std::optional<Side> DoubleGameWinnerSide(int declarer_card_points) {
  const int kTotalPoints = 94;
  const int kDoubleGameThreshold = 70;

  const int opponents_points = kTotalPoints - declarer_card_points;
  if (declarer_card_points > kDoubleGameThreshold) {
    return Side::kDeclarer;
  } else if (opponents_points > kDoubleGameThreshold) {
    return Side::kOpponents;
  } else {
    return std::nullopt;
  }
}

PagatUltimoResult PagatUltimoWinnerSide(const CommonState& game_data) {
  const bool pagat_in_last_trick =
      absl::c_find(game_data.tricks_.back(), kPagat) !=
      game_data.tricks_.back().end();
  const bool pagat_won = game_data.deck_[kPagat] ==
                         PlayerWonCardsLocation(game_data.pagat_holder_);

  if (!pagat_in_last_trick) {
    return PagatUltimoResult::kNotInLastTrick;
  } else if (pagat_won) {
    return PagatUltimoResult::kSucceeded;
  } else {
    return PagatUltimoResult::kFailed;
  }
}

std::optional<Side> TruletroaWinnerSide(const CommonState& game_data) {
  return CardSetWinnerSide(game_data, {kPagat, kXXI, kSkiz});
}

std::optional<Side> FourKingsWinnerSide(const CommonState& game_data) {
  return CardSetWinnerSide(game_data,
                           {MakeSuitCard(Suit::kHearts, SuitRank::kKing),
                            MakeSuitCard(Suit::kDiamonds, SuitRank::kKing),
                            MakeSuitCard(Suit::kClubs, SuitRank::kKing),
                            MakeSuitCard(Suit::kSpades, SuitRank::kKing)});
}

std::optional<Side> XXICatchWinnerSide(const CommonState& game_data) {
  bool same_trick = false;
  for (const CommonState::Trick& trick : game_data.tricks_) {
    if (absl::c_find(trick, kXXI) != trick.end() &&
        absl::c_find(trick, kSkiz) != trick.end()) {
      same_trick = true;
      break;
    }
  }
  const bool different_sides = game_data.player_sides_[game_data.XXI_holder_] !=
                               game_data.player_sides_[game_data.skiz_holder_];
  if (same_trick && different_sides) {
    return game_data.player_sides_[game_data.skiz_holder_];
  }
  return std::nullopt;
}

int AnnouncementMultiplier(const ScoringSummary& summary, Side side,
                           AnnouncementType type) {
  if (!summary.side_announced(side, type)) {
    return 1;
  }
  const CommonState::AnnouncementSide& announcement_side =
      side == Side::kDeclarer ? summary.declarer_side : summary.opponents_side;
  const int contra_level = announcement_side.contra_level_for(type);
  return 1 << (contra_level + 1);  // +1 for the announcement itself, then
                                   // doubling for each contra level
}

bool CanScoreAnnouncement(const ScoringSummary& summary, AnnouncementType type,
                          Side side) {
  bool made_volat = summary.volat_winner == side;
  bool announced = summary.side_announced(side, type);

  switch (type) {
    case AnnouncementType::kTuletroa:
    case AnnouncementType::kFourKings:
      return !made_volat || announced;
    case AnnouncementType::kDoubleGame: {
      bool volat_announced =
          summary.side_announced(side, AnnouncementType::kVolat);
      return (!made_volat && !volat_announced) || announced;
    }
    case AnnouncementType::kGame: {
      const bool double_or_volat_made =
          summary.volat_winner == side || summary.double_game_winner == side;
      const bool double_or_volat_announced =
          summary.side_announced(side, AnnouncementType::kDoubleGame) ||
          summary.side_announced(side, AnnouncementType::kVolat);
      return summary.game_contrad() ||
             (!double_or_volat_made && !double_or_volat_announced);
    }
    default:
      return true;
  }
}

void ApplyAnnouncementScore(int& declarer_score, const ScoringSummary& summary,
                            std::optional<Side> winner, int base_score,
                            AnnouncementType type) {
  for (Side side : {Side::kDeclarer, Side::kOpponents}) {
    int signed_score = base_score *
                       AnnouncementMultiplier(summary, side, type) *
                       (side == Side::kDeclarer ? 1 : -1);

    if (winner == side && CanScoreAnnouncement(summary, type, side)) {
      declarer_score += signed_score;
    }
    if (summary.side_announced(side, type) && winner != side) {
      declarer_score -= signed_score;
    }
  }
}

Side GameWinnerSide(int declarer_card_points) {
  return declarer_card_points > 47 ? Side::kDeclarer : Side::kOpponents;
}

void ApplyGameScore(int& declarer_score, const ScoringSummary& summary) {
  const int game_base_score = 4 - summary.winning_bid;
  const int signed_score =
      game_base_score *
      (1 << summary.declarer_side.contra_level_for(AnnouncementType::kGame)) *
      (summary.game_winner == Side::kDeclarer ? 1 : -1);

  if (CanScoreAnnouncement(summary, AnnouncementType::kGame,
                           summary.game_winner)) {
    declarer_score += signed_score;
  }
}

}  // namespace

ScoringSummary MakeScoringSummary(const CommonState& game_data) {
  ScoringSummary summary;
  summary.winning_bid = game_data.winning_bid_;
  summary.has_partner = game_data.partner_.has_value();
  summary.player_sides = game_data.player_sides_;
  summary.declarer_side = game_data.declarer_side_;
  SPIEL_CHECK_TRUE(
      game_data.declarer_side_.announced_for(AnnouncementType::kGame));
  summary.opponents_side = game_data.opponents_side_;
  int declarer_card_points = DeclarerCardPoints(game_data);
  summary.truletroa_winner = TruletroaWinnerSide(game_data);
  summary.four_kings_winner = FourKingsWinnerSide(game_data);
  summary.xxi_catch_winner = XXICatchWinnerSide(game_data);
  summary.double_game_winner = DoubleGameWinnerSide(declarer_card_points);
  summary.volat_winner = VolatWinnerSide(game_data);
  summary.game_winner = GameWinnerSide(declarer_card_points);
  summary.pagat_ultimo_result = PagatUltimoWinnerSide(game_data);
  summary.pagat_holder_side = game_data.player_sides_[game_data.pagat_holder_];
  return summary;
}

std::array<int, kNumPlayers> CalculateScores(const ScoringSummary& summary) {
  const int kGameBaseScore = 4 - summary.winning_bid;
  const int kTuletroaScore = 1;
  const int kFourKingsScore = 1;
  const int kPagatUltimoScore = 5;
  const int kXXICatchScore = 21;

  const int kVolatMultiplier = 3;
  const int kDoubleGameMultiplier = 2;

  int declarer_score = 0;

  ApplyAnnouncementScore(declarer_score, summary, summary.truletroa_winner,
                         kTuletroaScore, AnnouncementType::kTuletroa);
  ApplyAnnouncementScore(declarer_score, summary, summary.four_kings_winner,
                         kFourKingsScore, AnnouncementType::kFourKings);

  ApplyAnnouncementScore(declarer_score, summary, summary.xxi_catch_winner,
                         kXXICatchScore, AnnouncementType::kXXICapture);

  const bool pagatulti_announced = summary.side_announced(
      summary.pagat_holder_side, AnnouncementType::kPagatUltimo);

  if (summary.pagat_ultimo_result == PagatUltimoResult::kFailed &&
      !pagatulti_announced) {
    // Failed ulti without announcement: other side gets the score.
    if (summary.pagat_holder_side == Side::kDeclarer) {
      declarer_score -= kPagatUltimoScore;
    } else {
      declarer_score += kPagatUltimoScore;
    }
  }
  std::optional<Side> pagat_winner =
      summary.pagat_ultimo_result == PagatUltimoResult::kSucceeded
          ? std::make_optional(summary.pagat_holder_side)
          : std::nullopt;
  ApplyAnnouncementScore(declarer_score, summary, pagat_winner,
                         kPagatUltimoScore, AnnouncementType::kPagatUltimo);

  ApplyAnnouncementScore(declarer_score, summary, summary.volat_winner,
                         kGameBaseScore * kVolatMultiplier,
                         AnnouncementType::kVolat);

  ApplyAnnouncementScore(declarer_score, summary, summary.double_game_winner,
                         kGameBaseScore * kDoubleGameMultiplier,
                         AnnouncementType::kDoubleGame);

  // game needs special handling because it is always announced by the declarer
  // it is also always won by one side, but not always scored
  ApplyGameScore(declarer_score, summary);

  // Declarer played alone: each opponent pays/gets paid by them.
  const int declarer_base_score = declarer_score;
  const int opponents_score = -declarer_base_score;
  const int declarer_player_score =
      summary.has_partner ? declarer_base_score : declarer_base_score * 3;

  std::array<int, kNumPlayers> scores;
  scores.fill(0);
  for (Player player = 0; player < kNumPlayers; ++player) {
    scores[player] = (summary.player_sides[player] == Side::kDeclarer)
                         ? declarer_player_score
                         : opponents_score;
  }
  return scores;
}

}  // namespace hungarian_tarok
}  // namespace open_spiel
