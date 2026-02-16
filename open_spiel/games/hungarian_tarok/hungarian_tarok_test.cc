// Copyright 2019 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/hungarian_tarok/hungarian_tarok.h"

#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <vector>

#include "open_spiel/game_parameters.h"
#include "open_spiel/games/hungarian_tarok/phases.h"
#include "open_spiel/games/hungarian_tarok/scoring.h"
#include "open_spiel/observer.h"
#include "open_spiel/policy.h"
#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/tests/basic_tests.h"
#include "open_spiel/tests/console_play_test.h"

namespace open_spiel {
namespace hungarian_tarok {
namespace {

namespace testing = open_spiel::testing;
void PlayTalonSkartAndAnnulments(std::mt19937& mt,
                                 HungarianTarokState& post_bidding) {
  HungarianTarokState& state = post_bidding;
  while (state.GetPhaseType() == PhaseType::kTalon) {
    auto outcomes = state.ChanceOutcomes();
    Action action = SampleAction(outcomes, mt).first;
    state.ApplyAction(action);
  }
  while (state.GetPhaseType() == PhaseType::kAnnulments) {
    state.ApplyAction(kDontAnnul);
  }
  while (state.GetPhaseType() == PhaseType::kSkart) {
    auto legal_actions = state.LegalActions();
    SPIEL_CHECK_FALSE(legal_actions.empty());
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(legal_actions.size()) - 1);
    state.ApplyAction(legal_actions[dist(mt)]);
  }
  SPIEL_CHECK_EQ(state.GetPhaseType(), PhaseType::kAnnouncements);
}

void BasicHungarianTarokTests() {
  testing::LoadGameTest("hungarian_tarok");
  testing::ChanceOutcomesTest(*LoadGame("hungarian_tarok"));
  testing::RandomSimTest(*LoadGame("hungarian_tarok"), 500);
  // auto observer = LoadGame("hungarian_tarok")
  //                     ->MakeObserver(kDefaultObsType,
  //                                    GameParametersFromString("single_tensor"));
  // testing::RandomSimTestCustomObserver(*LoadGame("hungarian_tarok"),
  // observer);
}

void TrialThreeTest(std::mt19937& mt) {
  for (int i = 0; i < 200; ++i) {
    HungarianTarokState state = DealHelper().PostSetup(mt);
    bool has_honour = state.PlayerHoldsCard(3, kPagat) ||
                      state.PlayerHoldsCard(3, kSkiz) ||
                      state.PlayerHoldsCard(3, kXXI);
    if (has_honour) continue;

    while (state.GetPhaseType() == PhaseType::kBidding) {
      std::vector<Action> legal_actions = state.LegalActions();
      if (state.CurrentPlayer() == 3) {
        state.ApplyAction(Bid{3, false}.ToAction());
      } else {
        state.ApplyAction(Bid::PassAction());
      }
    }

    int count = 0;
    while (!state.IsTerminal() && state.GetPhaseType() == PhaseType::kTalon) {
      auto outcomes = state.ChanceOutcomes();
      Action action = SampleAction(outcomes, mt).first;
      state.ApplyAction(action);
      count++;
    }

    bool drew_honour = state.PlayerHoldsCard(3, kPagat) ||
                       state.PlayerHoldsCard(3, kSkiz) ||
                       state.PlayerHoldsCard(3, kXXI);
    if (drew_honour) {
      SPIEL_CHECK_FALSE(state.IsTerminal());
    } else {
      SPIEL_CHECK_TRUE(state.IsTerminal());
    }
  }
}

HungarianTarokState MakeState(std::mt19937& mt, Player pagat, Player xxi,
                              Player skiz, Player XVIII, Player XIX,
                              Player XX) {
  DealHelper deal_helper;
  deal_helper.SetCardDestination(kPagat, pagat);
  deal_helper.SetCardDestination(kSkiz, skiz);
  deal_helper.SetCardDestination(kXXI, xxi);
  deal_helper.SetCardDestination(MakeTarok(18), XVIII);
  deal_helper.SetCardDestination(MakeTarok(19), XIX);
  deal_helper.SetCardDestination(MakeTarok(20), XX);
  HungarianTarokState state = deal_helper.PostSetup(mt);
  return state;
}

HungarianTarokState TestBidSequence(
    HungarianTarokState state, std::mt19937& mt,
    std::vector<std::optional<Bid>> bids,
    std::optional<Card> called_card = std::nullopt) {
  for (std::optional<Bid> bid : bids) {
    if (!bid.has_value()) {
      state.ApplyAction(Bid::PassAction());
      continue;
    }

    SPIEL_CHECK_EQ(state.GetPhaseType(), PhaseType::kBidding);
    std::vector<Action> legal_actions = state.LegalActions();
    SPIEL_CHECK_TRUE(absl::c_find(legal_actions, bid->ToAction()) !=
                     legal_actions.end());
    state.ApplyAction(bid->ToAction());
  }
  SPIEL_CHECK_EQ(state.GetPhaseType(), PhaseType::kTalon);
  SPIEL_CHECK_EQ(state.MandatoryCalledCard(), called_card);
  return state;
}

void TestBids(std::mt19937& mt) {
  // straight XIX invit, accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/3, /*skiz=*/2,
                            /*XVIII=*/0, /*XIX=*/2, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      std::nullopt,
                      Bid{2, false},
                      Bid{1, false},
                  },
                  /*called_card=*/MakeTarok(19));
  // straight XVIII invit, accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/0, /*skiz=*/2,
                            /*XVIII=*/0, /*XIX=*/2, /*XX=*/2),
                  mt,
                  {
                      Bid{1, false},
                      std::nullopt,
                      Bid{0, false},
                      std::nullopt,
                  },
                  /*called_card=*/MakeTarok(18));
  // straight solo bid
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/1,
                            /*XVIII=*/1, /*XIX=*/2, /*XX=*/2),
                  mt,
                  {
                      Bid{0, false},
                      std::nullopt,
                      std::nullopt,
                      std::nullopt,
                  },
                  /*called_card=*/std::nullopt);
  // straight XIX invit, not accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/1,
                            /*XVIII=*/1, /*XIX=*/0, /*XX=*/2),
                  mt,
                  {
                      Bid{2, false},
                      std::nullopt,
                      std::nullopt,
                      std::nullopt,
                  },
                  /*called_card=*/std::nullopt);
  // XIX invit, accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/2,
                            /*XVIII=*/1, /*XIX=*/1, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      Bid{3, false},
                      Bid{2, false},
                      std::nullopt,

                      Bid{1, false},  // P1
                      Bid{1, true},   // P2
                  },
                  /*called_card=*/MakeTarok(19));
  // XIX invit, not accepted
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/1, /*skiz=*/2,
                            /*XVIII=*/1, /*XIX=*/1, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      Bid{3, false},
                      Bid{2, false},
                      std::nullopt,

                      Bid{1, false},  // P1
                      std::nullopt,   // P2
                  },
                  /*called_card=*/std::nullopt);
  // yielded game, XX called
  TestBidSequence(MakeState(mt,
                            /*pagat=*/0, /*xxi=*/2, /*skiz=*/3,
                            /*XVIII=*/1, /*XIX=*/1, /*XX=*/2),
                  mt,
                  {
                      std::nullopt,
                      std::nullopt,
                      Bid{3, false},
                      Bid{2, false},

                      std::nullopt,  // P2
                  },
                  /*called_card=*/MakeTarok(20));
  // no bids
  for (int i = 0; i < 10; ++i) {
    HungarianTarokState state = DealHelper().PostSetup(mt);
    while (!state.IsTerminal() && state.GetPhaseType() == PhaseType::kBidding) {
      state.ApplyAction(Bid::PassAction());
    }
    SPIEL_CHECK_TRUE(state.IsTerminal());
  }
}

void TestTuletroa(std::mt19937& mt) {
  const Action kAnnounceTuletroa =
      AnnouncementAction::AnnounceAction(AnnouncementType::kTuletroa);
  constexpr Player kPlayerXVIII = 1;
  constexpr Player kPlayerXIX = 2;
  constexpr Player kPlayerXX = 2;

  auto check_tuletroa = [&](Player pagat, Player xxi, Player skiz,
                            std::vector<std::optional<Bid>> bids, bool expected,
                            std::optional<Player> pass_until = std::nullopt,
                            std::optional<Card> called_card = std::nullopt) {
    HungarianTarokState state = TestBidSequence(
        MakeState(mt, pagat, xxi, skiz,
                  /*XVIII=*/kPlayerXVIII, /*XIX=*/kPlayerXIX, /*XX=*/kPlayerXX),
        mt, bids, called_card);
    PlayTalonSkartAndAnnulments(mt, state);
    state.ApplyAction(kAnnouncementsActionCallPartner);
    if (pass_until.has_value()) {
      while (state.CurrentPlayer() != *pass_until) {
        state.ApplyAction(AnnouncementAction::PassAction());
      }
    }
    std::vector<Action> legal_actions = state.LegalActions();
    bool has_tuletroa =
        absl::c_find(legal_actions, kAnnounceTuletroa) != legal_actions.end();
    SPIEL_CHECK_EQ(has_tuletroa, expected);
  };

  const std::vector<std::optional<Bid>> kSimpleBid = {
      std::nullopt, Bid{3, false}, std::nullopt, std::nullopt};
  const std::vector<std::optional<Bid>> kFullBid = {
      std::nullopt, Bid{3, false}, Bid{2, false}, Bid{1, false},
      std::nullopt,  // P1
      std::nullopt,  // P2
  };
  const std::vector<std::optional<Bid>> kXIXInvitBid = {
      std::nullopt, Bid{3, false}, Bid{1, false},
      std::nullopt, Bid{1, true},  // P1
  };
  const Player kDeclarer = 1;
  const Player kPartner = 2;

  // tuletroa as partner with XXI + Skiz
  check_tuletroa(/*pagat=*/0, /*xxi=*/1, /*skiz=*/1, kSimpleBid, true);
  // no tuletroa as declarer without XXI + Skiz
  check_tuletroa(/*pagat=*/0, /*xxi=*/1, /*skiz=*/2, kSimpleBid, false);

  // tuletroa as partner with skiz
  check_tuletroa(/*pagat=*/1, /*xxi=*/0, /*skiz=*/2, kSimpleBid, true,
                 /*pass_until=*/kPartner);

  // no tuletroa as partner with pagat
  check_tuletroa(/*pagat=*/2, /*xxi=*/1, /*skiz=*/0, kSimpleBid, false,
                 /*pass_until=*/kPartner);
  // tuletroa after full bid as declarer with Skiz
  check_tuletroa(/*pagat=*/2, /*xxi=*/1, /*skiz=*/3, kFullBid, true);

  // no tuletroa after full bid as declarer with XXI
  check_tuletroa(/*pagat=*/2, /*xxi=*/3, /*skiz=*/1, kFullBid, false);
  // tuletroa after full bid as partner with XXI
  check_tuletroa(/*pagat=*/1, /*xxi=*/2, /*skiz=*/3, kFullBid, true,
                 /*pass_until=*/kPartner);

  // tuletroa as declarer with XXI OR Skiz after cue bid
  check_tuletroa(/*pagat=*/2, /*xxi=*/1, /*skiz=*/3, kXIXInvitBid, true,
                 /*pass_until=*/kDeclarer, /*called_card=*/MakeTarok(19));
  // no tuletroa as declarer without XXI or Skiz after cue bid
  check_tuletroa(/*pagat=*/1, /*xxi=*/2, /*skiz=*/3, kXIXInvitBid, false,
                 /*pass_until=*/kDeclarer, /*called_card=*/MakeTarok(19));

  // tuletroa as partner (cue bidder) with two honours after cue bid
  check_tuletroa(/*pagat=*/2, /*xxi=*/2, /*skiz=*/1, kXIXInvitBid, true,
                 /*pass_until=*/kPartner, /*called_card=*/MakeTarok(19));
  // no tuletroa as partner (cue bidder) with one honour after cue bid
  check_tuletroa(/*pagat=*/1, /*xxi=*/2, /*skiz=*/3, kXIXInvitBid, false,
                 /*pass_until=*/kPartner, /*called_card=*/MakeTarok(19));
}

ScoringSummary MakeDefaultSummary() {
  ScoringSummary s;
  s.winning_bid = 3;
  s.has_partner = true;
  s.player_sides = {Side::kDeclarer, Side::kDeclarer, Side::kOpponents,
                    Side::kOpponents};
  // AnnouncementSide default-initializes to all false/0
  s.declarer_card_points = 50;
  s.truletroa_winner = std::nullopt;
  s.four_kings_winner = std::nullopt;
  s.xxi_catch_winner = std::nullopt;
  s.double_game_winner = std::nullopt;
  s.volat_winner = std::nullopt;
  s.pagat_ultimo_result = PagatUltimoResult::kNotInLastTrick;
  s.pagat_holder_side = Side::kDeclarer;
  return s;
}

void ScoringTest() {
  using Scores = std::array<int, kNumPlayers>;

  // Announced truletroa by declarer, but opponents won it.
  // Opponents win unannounced: -1*1. Declarer announced but lost: -1*2.
  // Total truletroa: -3. Game win: +1. Net: -2.
  {
    ScoringSummary s = MakeDefaultSummary();
    s.truletroa_winner = Side::kOpponents;
    s.declarer_side.announced[static_cast<int>(AnnouncementType::kTuletroa)] =
        true;
    SPIEL_CHECK_EQ(CalculateScores(s), (Scores{-2, -2, 2, 2}));
  }

  // Two game win, no partner with four kings for opponents (unannounced).
  // Game: 2, Four kings: -1, Net +1
  // Solo: declarer_score*=3 = 3, opponents each get -1.
  {
    ScoringSummary s = MakeDefaultSummary();
    s.winning_bid = 2;
    s.has_partner = false;
    s.player_sides = {Side::kDeclarer, Side::kOpponents, Side::kOpponents,
                      Side::kOpponents};
    s.declarer_card_points = 55;
    s.four_kings_winner = Side::kOpponents;
    SPIEL_CHECK_EQ(CalculateScores(s), (Scores{3, -1, -1, -1}));
  }

  // Failed pagat ultimo by declarer, unannounced.
  // Other side gets pagat score: -5. Game win: +1. Net: -4.
  {
    ScoringSummary s = MakeDefaultSummary();
    s.pagat_ultimo_result = PagatUltimoResult::kFailed;
    s.pagat_holder_side = Side::kDeclarer;
    SPIEL_CHECK_EQ(CalculateScores(s), (Scores{-4, -4, 4, 4}));
  }

  // Failed announced pagat ultimo by opponents + failed quiet ulti by declarer.
  // +10 for no opponent ulti, -5 for failed declarer ulti, +5 total.
  // Game loss: -1. Net: +4.
  {
    ScoringSummary s = MakeDefaultSummary();
    s.declarer_card_points = 40;
    s.pagat_ultimo_result = PagatUltimoResult::kFailed;
    s.pagat_holder_side = Side::kDeclarer;
    s.opponents_side
        .announced[static_cast<int>(AnnouncementType::kPagatUltimo)] = true;
    SPIEL_CHECK_EQ(CalculateScores(s), (Scores{4, 4, -4, -4}));
  }

  // Announced double game by declarer, contra'd once.
  // Double game score: 2*4*2=16.
  {
    ScoringSummary s = MakeDefaultSummary();
    s.winning_bid = 2;
    s.declarer_card_points = 75;
    s.double_game_winner = Side::kDeclarer;
    s.declarer_side.announced[static_cast<int>(AnnouncementType::kDoubleGame)] =
        true;
    s.declarer_side
        .contra_level[static_cast<int>(AnnouncementType::kDoubleGame)] = 1;
    SPIEL_CHECK_EQ(CalculateScores(s), (Scores{16, 16, -16, -16}));
  }

  // Volat for opponents suppresses quiet double game.
  // Volat base=1*3=3, opponents win: -3. Double game not scored (unannounced
  // and volat). Net: -3.
  {
    ScoringSummary s = MakeDefaultSummary();
    s.declarer_card_points = 8;
    s.double_game_winner = Side::kOpponents;
    s.volat_winner = Side::kOpponents;
    SPIEL_CHECK_EQ(CalculateScores(s), (Scores{-3, -3, 3, 3}));
  }
}

}  // namespace
}  // namespace hungarian_tarok
}  // namespace open_spiel

int main(int /*argc*/, char** /*argv*/) {
  std::mt19937 mt(42);

  open_spiel::hungarian_tarok::ScoringTest();
  open_spiel::hungarian_tarok::TestBids(mt);
  open_spiel::hungarian_tarok::TestTuletroa(mt);
  open_spiel::hungarian_tarok::TrialThreeTest(mt);
  open_spiel::hungarian_tarok::BasicHungarianTarokTests();
  // open_spiel::hungarian_tarok::ConsolePlayHungariantarokTest();
}
