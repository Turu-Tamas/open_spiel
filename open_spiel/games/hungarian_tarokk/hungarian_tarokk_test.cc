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

// The card helpers used by discarding and annulment (rules.md §4).
void CardHelperTest() {
  // The four kings are the highest card of each suit.
  SPIEL_CHECK_TRUE(IsKing(26));   // king of hearts
  SPIEL_CHECK_TRUE(IsKing(41));   // king of spades
  SPIEL_CHECK_FALSE(IsKing(25));  // queen of hearts
  SPIEL_CHECK_FALSE(IsKing(kCardSkiz));

  // Honours, kings and the XX may not be put in the skart; anything else may.
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardPagat));
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardXXI));
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardSkiz));
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardXX));
  SPIEL_CHECK_FALSE(IsDiscardableCard(26));       // a king
  SPIEL_CHECK_TRUE(IsDiscardableCard(kCardXIX));  // an ordinary tarokk
  SPIEL_CHECK_TRUE(IsDiscardableCard(25));        // a queen

  // Annullable holdings (rules.md §4.4); the rest of each hand is suit cards.
  SPIEL_CHECK_TRUE(
      HandIsAnnullable({26, 31, 36, 41, 22, 23, 24, 25, 27}));  // 4 kings
  SPIEL_CHECK_TRUE(
      HandIsAnnullable({22, 23, 24, 25, 26, 27, 28, 29, 30}));  // no tarokks
  SPIEL_CHECK_TRUE(HandIsAnnullable(
      {kCardXXI, 22, 23, 24, 25, 27, 28, 29, 30}));  // lone XXI
  SPIEL_CHECK_TRUE(HandIsAnnullable(
      {kCardPagat, 22, 23, 24, 25, 27, 28, 29, 30}));  // lone pagát
  SPIEL_CHECK_TRUE(HandIsAnnullable(
      {kCardPagat, kCardXXI, 22, 23, 24, 25, 27, 28, 30}));  // XXI + pagát

  SPIEL_CHECK_FALSE(
      HandIsAnnullable({5, 9, 22, 23, 24, 25, 26, 27, 28}));  // two tarokks
  SPIEL_CHECK_FALSE(HandIsAnnullable(
      {kCardXXI, 5, 22, 23, 24, 25, 27, 28, 30}));  // XXI + a tarokk
  SPIEL_CHECK_FALSE(
      HandIsAnnullable({26, 31, 36, 5, 22, 23, 24, 25, 27}));  // only 3 kings
}

// Drives the talon exchange and discarding through the game, using the
// deterministic deal (every chance node takes the lowest available card, so
// player p is dealt {p, p+4, ...} and player 0 holds the pagát).
void TalonDiscardTest() {
  std::shared_ptr<const Game> game = LoadGame("hungarian_tarokk");
  std::unique_ptr<State> state = game->NewInitialState();
  auto* ht = static_cast<HungarianTarokkState*>(state.get());

  while (state->IsChanceNode()) {
    state->ApplyAction(state->ChanceOutcomes().front().first);  // deal
  }

  // P0 bids three uncontested, then keeps it (the sole-bidder raise).
  SPIEL_CHECK_TRUE(ht->CurrentPhase() == Phase::kBidding);
  state->ApplyAction(kActionBidThree);  // P0
  state->ApplyAction(kActionPass);      // P1
  state->ApplyAction(kActionPass);      // P2
  state->ApplyAction(kActionPass);      // P3
  state->ApplyAction(kActionPass);      // P0 keeps the three
  SPIEL_CHECK_EQ(ht->Declarer(), 0);
  SPIEL_CHECK_TRUE(ht->WinningBid() == Bid::kThree);

  // The talon exchange begins with the draw (a chance step): with a "three" the
  // declarer draws 3 and each opponent draws 1.
  SPIEL_CHECK_TRUE(ht->CurrentPhase() == Phase::kTalonExchange);
  while (state->IsChanceNode()) {
    state->ApplyAction(state->ChanceOutcomes().front().first);
  }
  SPIEL_CHECK_EQ(static_cast<int>(ht->PlayerCards(0).size()), kHandSize + 3);
  for (Player p = 1; p < kNumPlayers; ++p) {
    SPIEL_CHECK_EQ(static_cast<int>(ht->PlayerCards(p).size()), kHandSize + 1);
  }

  // No annullable hand here, so the exchange goes straight to discarding (still
  // the same parent phase). Discard back to nine, always taking the first legal
  // discard, and check that no forbidden card (honour, king or XX) is offered.
  SPIEL_CHECK_TRUE(ht->CurrentPhase() == Phase::kTalonExchange);
  while (ht->CurrentPhase() == Phase::kTalonExchange) {
    std::vector<Action> legal = state->LegalActions();
    SPIEL_CHECK_FALSE(legal.empty());
    for (Action a : legal) {
      SPIEL_CHECK_TRUE(IsDiscardAction(a));
      SPIEL_CHECK_TRUE(IsDiscardableCard(CardForDiscardAction(a)));
    }
    state->ApplyAction(legal.front());
  }

  // Every hand is back to nine and the play has begun.
  for (Player p = 0; p < kNumPlayers; ++p) {
    SPIEL_CHECK_EQ(static_cast<int>(ht->PlayerCards(p).size()), kHandSize);
  }
  SPIEL_CHECK_TRUE(ht->CurrentPhase() == Phase::kPlaying);
  SPIEL_CHECK_EQ(state->CurrentPlayer(), 0);  // forehand leads

  // Play to the end; the placeholder scoring stays zero-sum.
  while (!state->IsTerminal()) {
    state->ApplyAction(state->LegalActions().front());
  }
  double sum = 0.0;
  for (double r : state->Returns()) sum += r;
  SPIEL_CHECK_TRUE(std::abs(sum) < 1e-9);
}

}  // namespace
}  // namespace hungarian_tarokk
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarokk::BiddingLogicTest();
  open_spiel::hungarian_tarokk::CardHelperTest();
  open_spiel::hungarian_tarokk::TalonDiscardTest();
  open_spiel::hungarian_tarokk::BasicHungarianTarokkTests();
}
