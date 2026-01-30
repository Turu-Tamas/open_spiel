#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_

#include "game_phase.h"
#include "announcements.h"
#include "bidding.h"

namespace open_spiel {
namespace hungarian_tarok {
  Side CardWinnerSide(const GameData& game_data, Card card) {
    return game_data.deck_[card] == WonCardsLocation(game_data.declarer_)
           || game_data.partner_ && game_data.deck_[card] == WonCardsLocation(*game_data.partner_)
               ? Side::kDeclarer
               : Side::kOpponents;
  }
  
  std::optional<Side> CardSetWinnerSide(const GameData& game_data, const std::vector<Card>& cards) {
    Side first_side = CardWinnerSide(game_data, cards[0]);
    for (Card card : cards) {
      Side side = CardWinnerSide(game_data, card);
      if (side != first_side) {
        return std::nullopt;
      }
    }
    return first_side;
  }

  std::optional<Side> VolatWinnerSide(const GameData& game_data) {
    Side first_side = game_data.player_sides_[game_data.trick_winners_.front()];
    return std::all_of(
               game_data.trick_winners_.begin(), game_data.trick_winners_.end(),
               [&](Player p) { return game_data.player_sides_[p] == first_side; })
               ? std::optional<Side>(first_side)
               : std::nullopt;
  }
  
  int DeclarerCardPoints(const GameData& game_data) {
    int points = 0;
    for (Card card = 0; card < kDeckSize; ++card) {
      Player owner = game_data.deck_[card];
      if (game_data.player_sides_[owner] == Side::kDeclarer) {
        points += CardPointValue(card);
      }
    }
    return points;
  }

  std::optional<Side> DoubleGameWinnerSide(const GameData& game_data) {
    const int kTotalPoints = 94;
    const int kDoubleGameThreshold = 70;

    int declarer_points = DeclarerCardPoints(game_data);
    int opponents_points = kTotalPoints - declarer_points;
    if (declarer_points > kDoubleGameThreshold) {
      return Side::kDeclarer;
    } else if (opponents_points > kDoubleGameThreshold) {
      return Side::kOpponents;
    } else {
      return std::nullopt;
    }
  }

  enum class PagatUltimoResult {
    kFailed, kSucceeded, kNotInLastTrick
  };
  PagatUltimoResult PagatUltimoWinnerSide(const GameData& game_data) {
    bool pagat_in_last_trick = absl::c_find(game_data.tricks_.back(), kPagat) == game_data.tricks_.back().end();
    Player trick_winner = game_data.trick_winners_.back();
    bool pagat_won = game_data.deck_[kPagat] == WonCardsLocation(trick_winner);

    if (!pagat_in_last_trick) {
      return PagatUltimoResult::kNotInLastTrick;
    } else if (pagat_won) {
      return PagatUltimoResult::kSucceeded;
    } else {
      return PagatUltimoResult::kFailed;
    }
  }

  std::optional<Side> TruletroaWinnerSide(const GameData& game_data) {
    return CardSetWinnerSide(game_data, {kPagat, kXXI, kSkiz});
  }

  std::optional<Side> FourKingsWinnerSide(const GameData& game_data) {
    return CardSetWinnerSide(
        game_data,
        {MakeSuitCard(Suit::kHearts, SuitRank::kKing),
         MakeSuitCard(Suit::kDiamonds, SuitRank::kKing),
         MakeSuitCard(Suit::kClubs, SuitRank::kKing),
         MakeSuitCard(Suit::kSpades, SuitRank::kKing)});
  }

  std::optional<Side> XXICatchWinnerSide(const GameData& game_data) {
    bool same_trick = false;
    for (const GameData::Trick &trick : game_data.tricks_) {
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

  std::array<int, kNumPlayers> CalculateScores(const GameData& game_data) {
    const int kGameBaseScore = 4 - game_data.winning_bid_;
    const int kTuletroaScore = 1;
    const int kFourKingsScore = 1;
    const int kPagatUltimoScore = 5;
    const int XXI_CatchScore = 21;

    const int kDoubleGameMultiplier = 2;
    const int kVolatMultiplier = 3;

    int declarer_score = 0;
    
    auto score_multiplier = [&](Side side, AnnouncementType type) {
      const GameData::AnnouncementSide &announcement_side =
          side == Side::kDeclarer ? game_data.declarer_side_ : game_data.opponents_side_;

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
    
    auto add_scores = [&](std::optional<Side> winner, int base_score, AnnouncementType type) {
      bool declarer_announced = game_data.declarer_side_.announced[static_cast<int>(type)];
      bool opponents_announced = game_data.opponents_side_.announced[static_cast<int>(type)];

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

    auto truletroa_winner = TruletroaWinnerSide(game_data);
    add_scores(truletroa_winner, kTuletroaScore, AnnouncementType::kTuletroa);

    auto four_kings_winner = FourKingsWinnerSide(game_data);
    add_scores(four_kings_winner, kFourKingsScore, AnnouncementType::kFourKings);

    auto XXI_catch_winner = XXICatchWinnerSide(game_data);
    add_scores(XXI_catch_winner, XXI_CatchScore, AnnouncementType::kXXICapture);

    PagatUltimoResult pagat_ultimo_result = PagatUltimoWinnerSide(game_data);
    Side pagat_side = game_data.player_sides_[game_data.pagat_holder_];
    Side other_side = pagat_side == Side::kDeclarer ? Side::kOpponents : Side::kDeclarer;
    if (pagat_ultimo_result == PagatUltimoResult::kSucceeded) {
      add_scores(pagat_side, kPagatUltimoScore, AnnouncementType::kPagatUltimo);
    } else if (pagat_ultimo_result == PagatUltimoResult::kFailed) {
      add_scores(other_side, kPagatUltimoScore, AnnouncementType::kPagatUltimo);
    } else { // kNotInLastTrick
      add_scores(std::nullopt, kPagatUltimoScore, AnnouncementType::kPagatUltimo);
    }
    
    auto double_game_winner = DoubleGameWinnerSide(game_data);
    
  }
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_