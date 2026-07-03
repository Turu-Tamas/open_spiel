#include "open_spiel/games/hungarian_tarokk/scoring.h"

#include <array>

#include "open_spiel/games/hungarian_tarokk/announcements.h"
#include "open_spiel/games/hungarian_tarokk/cards.h"
#include "open_spiel/spiel.h"

namespace open_spiel {
namespace hungarian_tarokk {
namespace {

int Pow2(int k) { return 1 << k; }

// The base game: exactly which of game / double game / volát scores, and how the
// announcements and the game kontra interact (§7.2, rules 1-6). Returns the
// declarer's-side signed value (positive = the declarer's side gains).
double BaseGameScore(const DealScore& d) {
  const int g = d.base_value;
  const int dpts = d.declarer_card_points;
  const int dtricks = d.declarer_tricks;

  auto announced = [&](Bonus b, Side s) {
    return d.announced[static_cast<int>(b)][static_cast<int>(s)];
  };
  auto kontra = [&](Bonus b, Side s) {
    return d.bonus_kontra[static_cast<int>(b)][static_cast<int>(s)];
  };
  auto sign = [](Side s) { return s == Side::kDeclarers ? 1 : -1; };

  // Per-side results. Double = that side took >= 71 (the other <= 23); volát =
  // that side won every trick.
  const std::array<bool, 2> double_made = {dpts >= 71, dpts <= 23};
  const std::array<bool, 2> volat_made = {dtricks == 9, dtricks == 0};

  double v = 0.0;

  // (A) Announced double / volát, scored win-or-lose per side with its own kontra
  //     chain. A made announced double additionally picks up a silent volát if
  //     all tricks were won and volát was not itself separately announced (a
  //     side that also announced volát scores that instead -- rule 3 vs rule 2).
  for (Side s : {Side::kDeclarers, Side::kDefenders}) {
    const int i = static_cast<int>(s);
    if (announced(Bonus::kVolat, s)) {
      v += sign(s) * 6 * g * Pow2(kontra(Bonus::kVolat, s)) *
           (volat_made[i] ? 1 : -1);
    }
    if (announced(Bonus::kDoubleGame, s)) {
      v += sign(s) * 4 * g * Pow2(kontra(Bonus::kDoubleGame, s)) *
           (double_made[i] ? 1 : -1);
      if (double_made[i] && volat_made[i] && !announced(Bonus::kVolat, s)) {
        v += sign(s) * 3 * g;
      }
    }
  }

  // (B) The ordinary game result, for the side that won the game. A side that
  //     announced a double/volát has its game represented by that announcement
  //     (win or lose), so it scores no separate ordinary game -- except that a
  //     kontra'd game is *always* scored on top (rules 5, 6).
  const Side winner = (dpts >= 48) ? Side::kDeclarers : Side::kDefenders;
  const int wsign = sign(winner);
  const int wlevel =
      volat_made[static_cast<int>(winner)]    ? 3
      : double_made[static_cast<int>(winner)] ? 2
                                              : 1;
  const bool winner_announced = announced(Bonus::kDoubleGame, winner) ||
                                announced(Bonus::kVolat, winner);
  if (!winner_announced) {
    if (d.game_kontra == 0) {
      // Silent one-of: game, silent double or silent volát by the bracket.
      v += wsign * g * wlevel;
    } else {
      // A kontra'd game is always scored (doubled); a silent double/volát is
      // added on top of it (rule 5).
      v += wsign * g * Pow2(d.game_kontra);
      if (wlevel >= 2) v += wsign * g * wlevel;
    }
  } else if (d.game_kontra >= 1) {
    v += wsign * g * Pow2(d.game_kontra);
  }
  return v;
}

// The flat bonuses and declarations (§7.3), independent of the base game value.
// Returns the declarer's-side signed value.
double FlatBonusScore(const DealScore& d) {
  auto announced = [&](Bonus b, Side s) {
    return d.announced[static_cast<int>(b)][static_cast<int>(s)];
  };
  auto kontra = [&](Bonus b, Side s) {
    return d.bonus_kontra[static_cast<int>(b)][static_cast<int>(s)];
  };
  auto sign = [](Side s) { return s == Side::kDeclarers ? 1 : -1; };
  const std::array<bool, 2> volat_made = {d.declarer_tricks == 9,
                                          d.declarer_tricks == 0};

  double v = 0.0;
  for (Side s : {Side::kDeclarers, Side::kDefenders}) {
    const int i = static_cast<int>(s);

    // Trull and four kings: announced pays ±2 (with kontra); a silent one pays
    // +1, but is suppressed if that side made volát -- winning every trick scores
    // volát only, not the silent trull/four kings it necessarily also made
    // (§5.2). (Announced trull/four kings still score under a volát.)
    auto card_bonus = [&](Bonus b, bool made) {
      if (announced(b, s)) {
        v += sign(s) * 2 * Pow2(kontra(b, s)) * (made ? 1 : -1);
      } else if (made && !volat_made[i]) {
        v += sign(s) * 1;
      }
    };
    card_bonus(Bonus::kTrull, d.trull_made[i]);
    card_bonus(Bonus::kFourKings, d.four_kings_made[i]);

    // Pagátultimó: announced pays ±10 (with kontra). Silently it pays +5 if this
    // side's pagát won the last trick, and -5 if the pagát was played to the last
    // trick and lost -- the silent failed ultimó is charged even with no
    // announcement (§5.2).
    if (announced(Bonus::kPagatUlti, s)) {
      const bool made = d.pagat_side == s && d.pagat_last_trick == 1;
      v += sign(s) * 10 * Pow2(kontra(Bonus::kPagatUlti, s)) * (made ? 1 : -1);
    } else if (d.pagat_side == s) {
      if (d.pagat_last_trick == 1) {
        v += sign(s) * 5;
      } else if (d.pagat_last_trick == -1) {
        v += sign(s) * -5;
      }
    }

    // XXI-catch: announced pays ±42, silent +21, to the catching side.
    const bool caught = d.xxi_caught && d.xxi_catcher_side == s;
    if (announced(Bonus::kXxiCatch, s)) {
      v += sign(s) * 42 * Pow2(kontra(Bonus::kXxiCatch, s)) * (caught ? 1 : -1);
    } else if (caught) {
      v += sign(s) * 21;
    }
  }

  // Tarokk-count declarations (§5.4): 8 -> 1, 9 -> 2, paid to the declaring side.
  for (Player p = 0; p < kNumPlayers; ++p) {
    const Side s = (p == d.declarer || p == d.partner) ? Side::kDeclarers
                                                       : Side::kDefenders;
    if (d.declared_tarokks[p] == 8) {
      v += sign(s) * 1;
    } else if (d.declared_tarokks[p] == 9) {
      v += sign(s) * 2;
    }
  }
  return v;
}

}  // namespace

std::array<double, kNumPlayers> ScoreDeal(const DealScore& deal) {
  // The declarer's-side value per opponent: base game plus the flat bonuses.
  const double v = BaseGameScore(deal) + FlatBonusScore(deal);

  std::array<double, kNumPlayers> ret{};
  if (deal.partner == kInvalidPlayer) {
    // Lone declarer: settles with each of the three opponents, so scores ×3
    // (§7.4). Everyone else pays/receives the per-opponent value v.
    ret[deal.declarer] = 3 * v;
    for (Player p = 0; p < kNumPlayers; ++p) {
      if (p != deal.declarer) ret[p] = -v;
    }
  } else {
    // Partnered: each player on the declarer's side receives v, each opponent
    // pays v (all four have the same magnitude, §7.4).
    for (Player p = 0; p < kNumPlayers; ++p) {
      const bool declarer_side = p == deal.declarer || p == deal.partner;
      ret[p] = declarer_side ? v : -v;
    }
  }
  return ret;
}

}  // namespace hungarian_tarokk
}  // namespace open_spiel
