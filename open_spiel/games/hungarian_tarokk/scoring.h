#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_SCORING_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_SCORING_H_

#include <array>

#include "open_spiel/games/hungarian_tarokk/announcements.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/spiel.h"

// Scoring one deal of Hungarian Tarokk (rules.md §7). The parent state extracts
// the trick-play and announcement facts into a `DealScore`, and `ScoreDeal`
// turns them into zero-sum per-player returns. Keeping the arithmetic in a pure
// function makes it unit-testable in isolation from the play.

namespace open_spiel {
namespace hungarian_tarokk {

// A safe (loose) bound on the magnitude of a single player's deal return, used
// for the game's MinUtility / MaxUtility.
inline constexpr double kMaxDealScore = 20000.0;

// Everything needed to score a completed deal. Sides are indexed by
// static_cast<int>(Side) (kDeclarers = 0, kDefenders = 1).
struct DealScore {
  Player declarer = kInvalidPlayer;
  // The declarer's partner, or kInvalidPlayer if the declarer plays alone (§7.4:
  // a lone declarer settles with each opponent, so scores ×3).
  Player partner = kInvalidPlayer;
  int base_value = 1;  // base game value by bid: three = 1 .. solo = 4

  // Card points (0..94) the declarer's side took in tricks plus its own skart
  // (§7.1); the opponents hold 94 - declarer_card_points.
  int declarer_card_points = 0;
  int declarer_tricks = 0;  // tricks won by the declarer's side (0..9)

  // Which side captured all three honours / all four kings in its tricks.
  std::array<bool, 2> trull_made{};
  std::array<bool, 2> four_kings_made{};
  // Pagátultimó: +1 = the pagát won the last trick, -1 = it was played to the
  // last trick and lost, 0 = it never reached the last trick. `pagat_side` is
  // the side that held (played) the pagát.
  int pagat_last_trick = 0;
  Side pagat_side = Side::kDeclarers;
  // XXI-catch: the Skíz captured the opponents' XXI (both in one trick, on
  // opposite sides); `xxi_catcher_side` is the Skíz's side.
  bool xxi_caught = false;
  Side xxi_catcher_side = Side::kDeclarers;

  // Announcements: whether each side announced each bonus and the kontra level of
  // that (bonus, side) claim; the game's own kontra level; and each player's
  // tarokk-count declaration (0, 8 or 9).
  std::array<std::array<bool, 2>, kNumBonuses> announced{};
  std::array<std::array<int, 2>, kNumBonuses> bonus_kontra{};
  int game_kontra = 0;
  std::array<int, kNumPlayers> declared_tarokks{};
};

// Per-player returns for the deal (§7.4), summing to zero.
std::array<double, kNumPlayers> ScoreDeal(const DealScore& deal);

}  // namespace hungarian_tarokk
}  // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROKK_SCORING_H_
