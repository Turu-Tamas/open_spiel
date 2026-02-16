#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include "open_spiel/abseil-cpp/absl/algorithm/container.h"
#include "open_spiel/games/hungarian_tarok/phases.h"

namespace open_spiel {
namespace hungarian_tarok {

enum class PagatUltimoResult { kFailed, kSucceeded, kNotInLastTrick };

struct ScoringSummary {
  int winning_bid;
  bool has_partner;
  std::array<Side, kNumPlayers> player_sides;
  CommonState::AnnouncementSide declarer_side;
  CommonState::AnnouncementSide opponents_side;

  int declarer_card_points;
  std::optional<Side> truletroa_winner;
  std::optional<Side> four_kings_winner;
  std::optional<Side> xxi_catch_winner;
  std::optional<Side> double_game_winner;
  std::optional<Side> volat_winner;
  PagatUltimoResult pagat_ultimo_result;
  Side pagat_holder_side;
};

inline Side CardWinnerSide(const CommonState& game_data, Card card) {
  return game_data.deck_[card] == PlayerWonCardsLocation(game_data.declarer_) ||
                 game_data.partner_ &&
                     game_data.deck_[card] ==
                         PlayerWonCardsLocation(*game_data.partner_)
             ? Side::kDeclarer
             : Side::kOpponents;
}

inline std::optional<Side> CardSetWinnerSide(const CommonState& game_data,
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

inline std::optional<Side> VolatWinnerSide(const CommonState& game_data) {
  Side first_side = game_data.player_sides_[game_data.trick_winners_.front()];
  return std::all_of(
             game_data.trick_winners_.begin(), game_data.trick_winners_.end(),
             [&](Player p) { return game_data.player_sides_[p] == first_side; })
             ? std::optional<Side>(first_side)
             : std::nullopt;
}

inline int DeclarerCardPoints(const CommonState& game_data) {
  int points = 0;
  for (Card card = 0; card < kDeckSize; ++card) {
    CardLocation location = game_data.deck_[card];
    if (IsWonCards(location)) {
      Player owner = WonCardsLocationPlayer(location);
      if (game_data.player_sides_[owner] == Side::kDeclarer) {
        points += CardPointValue(card);
      }
    }
  }
  return points;
}

inline std::optional<Side> DoubleGameWinnerSide(int declarer_card_points) {
  const int kTotalPoints = 94;
  const int kDoubleGameThreshold = 70;

  int opponents_points = kTotalPoints - declarer_card_points;
  if (declarer_card_points > kDoubleGameThreshold) {
    return Side::kDeclarer;
  } else if (opponents_points > kDoubleGameThreshold) {
    return Side::kOpponents;
  } else {
    return std::nullopt;
  }
}

inline PagatUltimoResult PagatUltimoWinnerSide(const CommonState& game_data) {
  bool pagat_in_last_trick = absl::c_find(game_data.tricks_.back(), kPagat) !=
                             game_data.tricks_.back().end();
  Player trick_winner = game_data.trick_winners_.back();
  bool pagat_won =
      game_data.deck_[kPagat] == PlayerWonCardsLocation(trick_winner);

  if (!pagat_in_last_trick) {
    return PagatUltimoResult::kNotInLastTrick;
  } else if (pagat_won) {
    return PagatUltimoResult::kSucceeded;
  } else {
    return PagatUltimoResult::kFailed;
  }
}

inline std::optional<Side> TruletroaWinnerSide(const CommonState& game_data) {
  return CardSetWinnerSide(game_data, {kPagat, kXXI, kSkiz});
}

inline std::optional<Side> FourKingsWinnerSide(const CommonState& game_data) {
  return CardSetWinnerSide(game_data,
                           {MakeSuitCard(Suit::kHearts, SuitRank::kKing),
                            MakeSuitCard(Suit::kDiamonds, SuitRank::kKing),
                            MakeSuitCard(Suit::kClubs, SuitRank::kKing),
                            MakeSuitCard(Suit::kSpades, SuitRank::kKing)});
}

inline std::optional<Side> XXICatchWinnerSide(const CommonState& game_data) {
  bool same_trick = false;
  for (const CommonState::Trick& trick : game_data.tricks_) {
    if (absl::c_find(trick, kXXI) != trick.end() &&
        absl::c_find(trick, kSkiz) != trick.end()) {
      same_trick = true;
      break;
    }
  }
  if (same_trick && game_data.deck_[kXXI] == game_data.deck_[kSkiz]) {
    return CardWinnerSide(game_data, kXXI);
  }
  return std::nullopt;
}

inline ScoringSummary MakeScoringSummary(const CommonState& game_data) {
  ScoringSummary summary;
  summary.winning_bid = game_data.winning_bid_;
  summary.has_partner = game_data.partner_.has_value();
  summary.player_sides = game_data.player_sides_;
  summary.declarer_side = game_data.declarer_side_;
  summary.opponents_side = game_data.opponents_side_;
  summary.declarer_card_points = DeclarerCardPoints(game_data);
  summary.truletroa_winner = TruletroaWinnerSide(game_data);
  summary.four_kings_winner = FourKingsWinnerSide(game_data);
  summary.xxi_catch_winner = XXICatchWinnerSide(game_data);
  summary.double_game_winner =
      DoubleGameWinnerSide(summary.declarer_card_points);
  summary.volat_winner = VolatWinnerSide(game_data);
  summary.pagat_ultimo_result = PagatUltimoWinnerSide(game_data);
  summary.pagat_holder_side = game_data.player_sides_[game_data.pagat_holder_];
  return summary;
}

inline std::array<int, kNumPlayers> CalculateScores(
    const ScoringSummary& summary) {
  const int kGameBaseScore = 4 - summary.winning_bid;
  const int kTuletroaScore = 1;
  const int kFourKingsScore = 1;
  const int kPagatUltimoScore = 5;
  const int kXxiCatchScore = 21;

  const int kDoubleGameMultiplier = 2;
  const int kVolatMultiplier = 3;

  int declarer_score = 0;

  auto score_multiplier = [&](Side side, AnnouncementType type) {
    const CommonState::AnnouncementSide& announcement_side =
        side == Side::kDeclarer ? summary.declarer_side
                                : summary.opponents_side;

    int multiplier = 1;
    if (announcement_side.announced[static_cast<int>(type)]) {
      multiplier = 2;
    } else {
      return 1;
    }

    int contra_level = announcement_side.contra_level[static_cast<int>(type)];
    for (int i = 0; i < contra_level; ++i) {
      multiplier *= 2;
    }
    return multiplier;
  };

  auto add_scores = [&](std::optional<Side> winner, int base_score,
                        AnnouncementType type) {
    bool declarer_announced =
        summary.declarer_side.announced[static_cast<int>(type)];
    bool opponents_announced =
        summary.opponents_side.announced[static_cast<int>(type)];

    if (winner == Side::kDeclarer) {
      declarer_score += base_score * score_multiplier(Side::kDeclarer, type);
    } else if (winner == Side::kOpponents) {
      declarer_score -= base_score * score_multiplier(Side::kOpponents, type);
    }

    if (declarer_announced && winner != Side::kDeclarer) {
      declarer_score -= base_score * score_multiplier(Side::kDeclarer, type);
    }
    if (opponents_announced && winner != Side::kOpponents) {
      declarer_score += base_score * score_multiplier(Side::kOpponents, type);
    }
  };

  add_scores(summary.truletroa_winner, kTuletroaScore,
             AnnouncementType::kTuletroa);

  add_scores(summary.four_kings_winner, kFourKingsScore,
             AnnouncementType::kFourKings);

  add_scores(summary.xxi_catch_winner, kXxiCatchScore,
             AnnouncementType::kXXICapture);

  const bool pagatulti_announced =
      summary.pagat_holder_side == Side::kDeclarer
          ? summary.declarer_side
                .announced[static_cast<int>(AnnouncementType::kPagatUltimo)]
          : summary.opponents_side
                .announced[static_cast<int>(AnnouncementType::kPagatUltimo)];

  if (summary.pagat_ultimo_result == PagatUltimoResult::kFailed &&
      !pagatulti_announced) {
    // failed ulti without announcement: other side gets the score
    if (summary.pagat_holder_side == Side::kDeclarer) {
      declarer_score -= kPagatUltimoScore;
    } else {
      declarer_score += kPagatUltimoScore;
    }
  }
  if (summary.pagat_ultimo_result == PagatUltimoResult::kSucceeded) {
    add_scores(summary.pagat_holder_side, kPagatUltimoScore,
               AnnouncementType::kPagatUltimo);
  } else {
    // kNotInLastTrick or kFailed (even if the pagat failed, if the other
    // announced ulti, they also lose the score)
    add_scores(std::nullopt, kPagatUltimoScore, AnnouncementType::kPagatUltimo);
  }

  const CommonState::AnnouncementSide& double_side =
      summary.double_game_winner == Side::kDeclarer ? summary.declarer_side
                                                    : summary.opponents_side;
  add_scores(summary.volat_winner, kGameBaseScore * kVolatMultiplier,
             AnnouncementType::kVolat);
  // Don't score unannounced double games if volat.
  if (!summary.volat_winner.has_value() ||
      double_side.announced[static_cast<int>(AnnouncementType::kDoubleGame)]) {
    add_scores(summary.double_game_winner,
               kGameBaseScore * kDoubleGameMultiplier,
               AnnouncementType::kDoubleGame);
  }

  if (!summary.double_game_winner.has_value() &&
      !summary.volat_winner.has_value()) {
    // normal game scoring
    if (summary.declarer_card_points > 47) {
      declarer_score += kGameBaseScore;
    } else {
      declarer_score -= kGameBaseScore;
    }
  }

  // declarer played alone, everyone pays/gets paid by them
  int opponents_score = -declarer_score;
  if (!summary.has_partner) {
    declarer_score *= 3;
  }
  std::array<int, kNumPlayers> scores;
  scores.fill(0);
  for (Player player = 0; player < kNumPlayers; ++player) {
    scores[player] = (summary.player_sides[player] == Side::kDeclarer)
                         ? declarer_score
                         : opponents_score;
  }
  return scores;
}
}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_
