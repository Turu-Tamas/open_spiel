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

#include "open_spiel/games/hungarian_tarokk/hungarian_tarokk.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "open_spiel/spiel.h"
#include "open_spiel/spiel_utils.h"
#include "open_spiel/tests/basic_tests.h"

namespace open_spiel {
namespace hungarian_tarokk {
namespace {

namespace testing = open_spiel::testing;

// Standard OpenSpiel sanity checks: the game loads, the chance outcomes are
// well-formed, and many random playthroughs respect the generic API contracts.
void BasicHungarianTarokkTests() {
  testing::LoadGameTest("hungarian_tarokk");
  testing::ChanceOutcomesTest(*LoadGame("hungarian_tarokk"));
  testing::RandomSimTest(*LoadGame("hungarian_tarokk"), 100);
}

// Every player holds a (plain) honour and nothing else, unless tweaked.
std::vector<PlayerBidInfo> FourHonours() {
  std::vector<PlayerBidInfo> info(kNumPlayers);
  for (PlayerBidInfo& i : info) i.has_honour = true;
  return info;
}

bool Contains(const std::vector<Action>& v, Action a) {
  return std::find(v.begin(), v.end(), a) != v.end();
}

// Scripted auctions exercising the BiddingState directly.
void BiddingLogicTest() {
  // An uncontested plain three lets the sole bidder raise (§3.2); here P0 keeps
  // the three.
  {
    BiddingState b(FourHonours());
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 0);
    b.ApplyAction(kActionBidThree);                    // P0
    b.ApplyAction(kActionPass);                        // P1
    b.ApplyAction(kActionPass);                        // P2
    b.ApplyAction(kActionPass);                        // P3
    // Not finished: P0 may raise the uncontested three.
    SPIEL_CHECK_FALSE(b.IsFinished());
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 0);
    SPIEL_CHECK_EQ(static_cast<int>(b.LegalActions().size()), 4);
    b.ApplyAction(kActionPass);                        // keep the three
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_FALSE(b.PassedOut());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kThree);
  }

  // Same, but the sole bidder raises to solo.
  {
    BiddingState b(FourHonours());
    b.ApplyAction(kActionBidThree);                    // P0
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionBidSolo);                     // raise three -> solo
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kSolo);
  }

  // Holding: P0 three, P1 two, P2/P3 pass, P0 holds the two, P1 passes.
  {
    BiddingState b(FourHonours());
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionBidTwo);    // P1
    b.ApplyAction(kActionPass);      // P2
    b.ApplyAction(kActionPass);      // P3
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 0);
    SPIEL_CHECK_TRUE(Contains(b.LegalActions(), kActionHold));
    b.ApplyAction(kActionHold);      // P0 holds the two
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 1);
    b.ApplyAction(kActionPass);      // P1
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kTwo);
  }

  // A player with no honour must pass.
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_honour = false;
    BiddingState b(info);
    std::vector<Action> legal = b.LegalActions();
    SPIEL_CHECK_EQ(static_cast<int>(legal.size()), 1);
    SPIEL_CHECK_EQ(legal[0], kActionPass);
  }

  // Everybody passes -> passed-out hand, no declarer.
  {
    BiddingState b(FourHonours());
    for (int i = 0; i < kNumPlayers; ++i) b.ApplyAction(kActionPass);
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_TRUE(b.PassedOut());
    SPIEL_CHECK_EQ(b.Declarer(), kInvalidPlayer);
  }

  // Cue bid: P0 three, P1/P2 pass, P3 bids one (a single jump = cue of the XIX),
  // P0 overcalls solo and wins, so must call P3's XIX (§3.3).
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[3].has_xix = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionPass);      // P1
    b.ApplyAction(kActionPass);      // P2
    SPIEL_CHECK_TRUE(Contains(b.LegalActions(), kActionBidOne));   // cue XIX
    b.ApplyAction(kActionBidOne);    // P3 cue bids the XIX
    SPIEL_CHECK_EQ(b.CueBidder(), 3);
    SPIEL_CHECK_TRUE(b.CuedCard() == CalledCard::kXIX);
    b.ApplyAction(kActionBidSolo);   // P0 overcalls
    b.ApplyAction(kActionPass);      // P3 passes
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kSolo);
    SPIEL_CHECK_TRUE(b.ObligatoryCalledCard() == CalledCard::kXIX);
  }

  // Cue bids are illegal without the promised card: P3 lacks the XIX/XVIII, so
  // its only positive bid here is the (non-jump) two.
  {
    BiddingState b(FourHonours());  // nobody holds XIX/XVIII
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionPass);      // P1
    b.ApplyAction(kActionPass);      // P2
    std::vector<Action> legal = b.LegalActions();  // P3
    SPIEL_CHECK_TRUE(Contains(legal, kActionBidTwo));    // minimum overbid
    SPIEL_CHECK_FALSE(Contains(legal, kActionBidOne));   // would be a cue XIX
    SPIEL_CHECK_FALSE(Contains(legal, kActionBidSolo));  // would be a cue XVIII
  }

  // Yielded game: P0 three, P1 two, P2/P3 pass, P0 passes (a yield) and so must
  // hold the XX + a high honour; the declarer (P1) must call the XX (§3.4).
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_xx = true;
    info[0].has_high_honour = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionBidTwo);    // P1
    b.ApplyAction(kActionPass);      // P2
    b.ApplyAction(kActionPass);      // P3
    SPIEL_CHECK_TRUE(Contains(b.LegalActions(), kActionPass));  // yield allowed
    b.ApplyAction(kActionPass);      // P0 yields
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 1);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kTwo);
    SPIEL_CHECK_TRUE(b.Yielded());
    SPIEL_CHECK_TRUE(b.ObligatoryCalledCard() == CalledCard::kXX);
  }

  // A yield is illegal without the XX + a high honour: P0 must hold instead.
  {
    BiddingState b(FourHonours());  // P0 lacks the XX
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionBidTwo);    // P1
    b.ApplyAction(kActionPass);      // P2
    b.ApplyAction(kActionPass);      // P3
    std::vector<Action> legal = b.LegalActions();  // P0 in the yield position
    SPIEL_CHECK_FALSE(Contains(legal, kActionPass));  // cannot yield
    SPIEL_CHECK_TRUE(Contains(legal, kActionHold));   // must hold (or cue bid)
  }

  // Trial bid (§3.5): first three pass, the fourth seat may bid three even with
  // no honour.
  {
    std::vector<PlayerBidInfo> info(kNumPlayers);  // nobody holds an honour
    BiddingState b(info);
    b.ApplyAction(kActionPass);  // P0
    b.ApplyAction(kActionPass);  // P1
    b.ApplyAction(kActionPass);  // P2
    std::vector<Action> legal = b.LegalActions();  // P3
    SPIEL_CHECK_EQ(static_cast<int>(legal.size()), 2);
    SPIEL_CHECK_EQ(legal[0], kActionPass);
    SPIEL_CHECK_EQ(legal[1], kActionBidThree);
    b.ApplyAction(kActionBidThree);  // trial bid
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 3);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kThree);
  }
}

// Drives a full game through dealing and bidding into the play, then to the end.
void GameBiddingIntegrationTest() {
  std::shared_ptr<const Game> game = LoadGame("hungarian_tarokk");
  std::unique_ptr<State> state = game->NewInitialState();

  // Deal (always taking the lowest available card -> player 0 holds the pagát).
  while (state->IsChanceNode()) {
    state->ApplyAction(state->ChanceOutcomes().front().first);
  }

  // Bidding: always take the strongest available action (the last legal one),
  // which forces player 0 to bid solo and leaves the others no reply but pass.
  auto* ht = static_cast<HungarianTarokkState*>(state.get());
  SPIEL_CHECK_TRUE(ht->CurrentPhase() == Phase::kBidding);
  while (ht->CurrentPhase() == Phase::kBidding) {
    state->ApplyAction(state->LegalActions().back());
  }
  SPIEL_CHECK_TRUE(ht->CurrentPhase() == Phase::kPlaying);
  SPIEL_CHECK_EQ(ht->Declarer(), 0);
  SPIEL_CHECK_TRUE(ht->WinningBid() == Bid::kSolo);
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 0);  // forehand leads

  // Play out the nine tricks; the placeholder scoring stays zero-sum.
  int moves = 0;
  while (!state->IsTerminal()) {
    SPIEL_CHECK_FALSE(state->IsChanceNode());
    state->ApplyAction(state->LegalActions().front());
    ++moves;
  }
  SPIEL_CHECK_EQ(moves, kNumTricks * kNumPlayers);
  double sum = 0.0;
  for (double r : state->Returns()) sum += r;
  SPIEL_CHECK_TRUE(std::abs(sum) < 1e-9);
}

}  // namespace
}  // namespace hungarian_tarokk
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarokk::BiddingLogicTest();
  open_spiel::hungarian_tarokk::GameBiddingIntegrationTest();
  open_spiel::hungarian_tarokk::BasicHungarianTarokkTests();
}
