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

  std::optional<Side> truletroa_winner;
  std::optional<Side> four_kings_winner;
  std::optional<Side> xxi_catch_winner;
  std::optional<Side> double_game_winner;
  std::optional<Side> volat_winner;
  Side game_winner;
  PagatUltimoResult pagat_ultimo_result;
  Side pagat_holder_side;

  bool declarer_announced(AnnouncementType type) const;
  bool opponents_announced(AnnouncementType type) const;
  bool side_announced(Side side, AnnouncementType type) const;
  bool game_contrad() const;
};

ScoringSummary MakeScoringSummary(const CommonState& game_data);

std::array<int, kNumPlayers> CalculateScores(const ScoringSummary& summary);
}  // namespace hungarian_tarok
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SCORING_H_
