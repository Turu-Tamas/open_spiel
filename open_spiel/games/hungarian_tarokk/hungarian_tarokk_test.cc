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
  // An uncontested plain three lets the sole bidder raise.
  // Here P0 keeps the three.
  {
    BiddingState b(FourHonours());
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 0);
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionPass);      // P1
    b.ApplyAction(kActionPass);      // P2
    b.ApplyAction(kActionPass);      // P3
    // Not finished: P0 may raise the uncontested three.
    SPIEL_CHECK_FALSE(b.IsFinished());
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 0);
    SPIEL_CHECK_EQ(static_cast<int>(b.LegalActions().size()), 4);
    b.ApplyAction(kActionPass);  // keep the three
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_FALSE(b.PassedOut());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kThree);
  }

  // Same, but the sole bidder raises to solo.
  {
    BiddingState b(FourHonours());
    b.ApplyAction(kActionBidThree);  // P0
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionBidSolo);  // raise three -> solo
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
    SPIEL_CHECK_EQ(std::vector<Action>{kActionHold}, b.LegalActions());
    b.ApplyAction(kActionHold);  // P0 holds the two
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 1);
    b.ApplyAction(kActionPass);  // P1
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

  // Cue bid: P0 three, P1/P2 pass, P3 bids one (a single jump = cue of the
  // XIX), P0 overcalls solo and wins, so must call P3's XIX (§3.3).
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[3].has_xix = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidThree);                               // P0
    b.ApplyAction(kActionPass);                                   // P1
    b.ApplyAction(kActionPass);                                   // P2
    SPIEL_CHECK_TRUE(Contains(b.LegalActions(), kActionBidOne));  // cue XIX
    b.ApplyAction(kActionBidOne);  // P3 cue bids the XIX
    SPIEL_CHECK_EQ(b.CueBidder(), 3);
    SPIEL_CHECK_TRUE(b.CuedCard() == CalledCard::kXIX);
    // Having invited, P3 takes no further part: it is P0's turn, not P3's.
    SPIEL_CHECK_EQ(b.CurrentPlayer(), 0);
    b.ApplyAction(kActionBidSolo);     // P0 overcalls and wins outright
    SPIEL_CHECK_TRUE(b.IsFinished());  // no final pass by P3 is needed
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kSolo);
    SPIEL_CHECK_TRUE(b.ObligatoryCalledCard() == CalledCard::kXIX);
  }

  // Two players bidding after a cue bid: P0 opens two (a cue of the XIX), then
  // P1 and P2 compete over it; P2 wins and must call the cue-bidder P0's XIX.
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_xix = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidTwo);  // P0
    SPIEL_CHECK_EQ(b.CueBidder(), 0);
    SPIEL_CHECK_TRUE(b.CuedCard() == CalledCard::kXIX);
    b.ApplyAction(kActionBidOne);   // P1 overcalls
    b.ApplyAction(kActionBidSolo);  // P2 overcalls
    b.ApplyAction(kActionPass);     // P3 passes
    SPIEL_CHECK_EQ(b.CurrentPlayer(),
                   1);           // back to P1, P0 is skipped (cue-bidder)
    b.ApplyAction(kActionPass);  // P1 passes; P2 wins
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 2);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kSolo);
    SPIEL_CHECK_TRUE(b.ObligatoryCalledCard() == CalledCard::kXIX);
  }

  // Cue bids are illegal without the promised card: P3 lacks the XIX/XVIII, so
  // its only positive bid here is the (non-jump) two.
  {
    BiddingState b(FourHonours());                 // nobody holds XIX/XVIII
    b.ApplyAction(kActionBidThree);                // P0
    b.ApplyAction(kActionPass);                    // P1
    b.ApplyAction(kActionPass);                    // P2
    std::vector<Action> legal = b.LegalActions();  // P3
    // Only pass and the (non-jump) two -- no cue bids of the one/solo.
    std::vector<Action> expected = {kActionPass, BidToAction(Bid::kTwo)};
    SPIEL_CHECK_EQ(expected, legal);
  }

  // An opening solo is NOT a cue bid, as nobody can bid after it: P0 (holding
  // no XIX/XVIII) may open solo, wins, and there is no cued / obligatory card.
  {
    BiddingState b(FourHonours());  // P0 holds no XIX/XVIII
    SPIEL_CHECK_TRUE(Contains(b.LegalActions(), kActionBidSolo));
    b.ApplyAction(kActionBidSolo);                  // P0 opens solo
    SPIEL_CHECK_EQ(b.CueBidder(), kInvalidPlayer);  // not a cue bid
    b.ApplyAction(kActionPass);                     // P1
    b.ApplyAction(kActionPass);                     // P2
    b.ApplyAction(kActionPass);                     // P3
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kSolo);
    SPIEL_CHECK_FALSE(b.Yielded());
    SPIEL_CHECK_TRUE(b.ObligatoryCalledCard() == CalledCard::kNone);
  }

  // Yielded game: P0 three, P1 two, P2/P3 pass, P0 passes (a yield) and so must
  // hold the XX + a high honour; the declarer (P1) must call the XX (§3.4).
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_xx = true;
    info[0].has_high_honour = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidThree);                             // P0
    b.ApplyAction(kActionBidTwo);                               // P1
    b.ApplyAction(kActionPass);                                 // P2
    b.ApplyAction(kActionPass);                                 // P3
    SPIEL_CHECK_TRUE(Contains(b.LegalActions(), kActionPass));  // yield allowed
    b.ApplyAction(kActionPass);                                 // P0 yields
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 1);
    SPIEL_CHECK_TRUE(b.WinningBid() == Bid::kTwo);
    SPIEL_CHECK_TRUE(b.Yielded());
    SPIEL_CHECK_TRUE(b.ObligatoryCalledCard() == CalledCard::kXX);
  }

  // A yield is illegal without the XX + a high honour: P0 must hold instead.
  {
    BiddingState b(FourHonours());                 // P0 lacks the XX
    b.ApplyAction(kActionBidThree);                // P0
    b.ApplyAction(kActionBidTwo);                  // P1
    b.ApplyAction(kActionPass);                    // P2
    b.ApplyAction(kActionPass);                    // P3
    std::vector<Action> legal = b.LegalActions();  // P0 in the yield position
    SPIEL_CHECK_FALSE(Contains(legal, kActionPass));  // cannot yield
    SPIEL_CHECK_TRUE(Contains(legal, kActionHold));   // must hold (or cue bid)
  }

  // Yielding is illegal with only the pagát: P0 holds the XX but no HIGH honour
  // (its honour is the pagát), so it cannot yield and must hold (§3.4, §5.1.3).
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_xx = true;
    info[0].has_high_honour = false;  // P0's only honour is the pagát
    BiddingState b(info);
    b.ApplyAction(kActionBidThree);                // P0
    b.ApplyAction(kActionBidTwo);                  // P1
    b.ApplyAction(kActionPass);                    // P2
    b.ApplyAction(kActionPass);                    // P3
    std::vector<Action> legal = b.LegalActions();  // P0 in the yield position
    SPIEL_CHECK_FALSE(Contains(legal, kActionPass));  // cannot yield with pagát
    SPIEL_CHECK_TRUE(Contains(legal, kActionHold));
  }

  // Trial bid (§3.5): first three pass, the fourth seat may bid three even with
  // no honour.
  {
    std::vector<PlayerBidInfo> info(kNumPlayers);  // nobody holds an honour
    BiddingState b(info);
    b.ApplyAction(kActionPass);                    // P0
    b.ApplyAction(kActionPass);                    // P1
    b.ApplyAction(kActionPass);                    // P2
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

}  // namespace
}  // namespace hungarian_tarokk
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarokk::BiddingLogicTest();
  open_spiel::hungarian_tarokk::BasicHungarianTarokkTests();
}
