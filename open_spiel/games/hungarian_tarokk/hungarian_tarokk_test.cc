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
  // Honours, kings and the XX may not be put in the skart; anything else may.
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardPagat));
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardXXI));
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardSkiz));
  SPIEL_CHECK_FALSE(IsDiscardableCard(kCardXX));
  SPIEL_CHECK_FALSE(IsDiscardableCard(MakeKing(kHearts)));
  SPIEL_CHECK_TRUE(IsDiscardableCard(kCardXIX));  // an ordinary tarokk
  SPIEL_CHECK_TRUE(IsDiscardableCard(SuitCard(kHearts, kQueen)));

  // Annullable holdings (rules.md §4.4). HandIsAnnullable ignores hand size, so
  // each case lists only the cards that matter.
  SPIEL_CHECK_TRUE(HandIsAnnullable({MakeKing(kHearts), MakeKing(kDiamonds),
                                     MakeKing(kClubs), MakeKing(kSpades)}));
  SPIEL_CHECK_TRUE(HandIsAnnullable(
      {SuitCard(kHearts, kQueen), SuitCard(kDiamonds, kJack)}));  // no tarokks
  SPIEL_CHECK_TRUE(HandIsAnnullable({kCardXXI, SuitCard(kHearts, kLow)}));
  SPIEL_CHECK_TRUE(HandIsAnnullable({kCardPagat, SuitCard(kHearts, kLow)}));
  SPIEL_CHECK_TRUE(
      HandIsAnnullable({kCardPagat, kCardXXI, SuitCard(kHearts, kLow)}));

  SPIEL_CHECK_FALSE(HandIsAnnullable({Tarokk(6), Tarokk(10)}));  // two tarokks
  SPIEL_CHECK_FALSE(HandIsAnnullable({kCardXXI, Tarokk(6)}));  // XXI + a tarokk
  SPIEL_CHECK_FALSE(
      HandIsAnnullable({MakeKing(kHearts), MakeKing(kDiamonds),
                        MakeKing(kClubs), Tarokk(6)}));  // 3 kings
}

// Completes a valid 42-card deal: player 0 gets `hand0`. `hand0` and the
// talon must be disjoint and contain 9 and 6 cards, respectively.
std::vector<std::vector<Action>> DealWith(const std::vector<Action>& hand0,
                                          const std::vector<Action>& talon) {
  std::vector<bool> used(kNumCards, false);
  for (Action c : hand0) used[c] = true;
  for (Action c : talon) used[c] = true;
  std::vector<std::vector<Action>> hands(kNumPlayers);
  hands[0] = hand0;
  int p = 1;
  for (Action c = 0; c < kNumCards; ++c) {
    if (used[c]) continue;
    if (static_cast<int>(hands[p].size()) == kHandSize) ++p;
    hands[p].push_back(c);
  }
  return hands;
}

// Runs the talon draw (the chance step) by always taking the lowest card.
void DrawTalon(TalonExchangeState* t) {
  while (t->CurrentPlayer() == kChancePlayerId) {
    t->ApplyAction(t->ChanceOutcomes().front().first);
  }
}

// The lowest `count` card ids not already in `hand`, as a talon disjoint from
// it (the exact cards do not matter, only that they are valid and distinct).
std::vector<Action> UnusedTalon(const std::vector<Action>& hand) {
  std::vector<bool> used(kNumCards, false);
  for (Action c : hand) used[c] = true;
  std::vector<Action> talon;
  for (Action c = 0;
       c < kNumCards && static_cast<int>(talon.size()) < kTalonSize; ++c) {
    if (!used[c]) talon.push_back(c);
  }
  return talon;
}

// Engineered talon exchanges exercising discard legality and annulment.
void TalonLogicTest() {
  // Discard legality: P0 (declarer, "three") holds the pagát/XXI/Skíz, the XX
  // and a king among ordinary tarokks; after drawing it may discard none of
  // those, but may discard the ordinary cards.
  {
    std::vector<Action> hand0 = {kCardPagat, Tarokk(2), Tarokk(3),
                                 Tarokk(4),  Tarokk(5), kCardXX,
                                 kCardXXI,   kCardSkiz, MakeKing(kHearts)};
    std::vector<Action> talon = UnusedTalon(hand0);
    TalonExchangeState t(DealWith(hand0, talon), talon, /*declarer=*/0,
                         Bid::kThree, kInvalidPlayer, CalledCard::kNone);
    DrawTalon(&t);
    while (t.LegalActions().front() == kActionAnnul) {
      t.ApplyAction(kActionDeclineAnnul);  // skip any opponent's throw-in
    }
    SPIEL_CHECK_EQ(t.CurrentPlayer(), 0);
    std::vector<Action> legal = t.LegalActions();
    std::vector<Action> forbidden = {kCardPagat, kCardXXI, kCardSkiz, kCardXX,
                                     MakeKing(kHearts)};
    for (Action f : forbidden) {
      SPIEL_CHECK_FALSE(Contains(legal, DiscardActionForCard(f)));
    }
    for (Action ok : {Tarokk(2), Tarokk(3), Tarokk(4), Tarokk(5)}) {
      SPIEL_CHECK_TRUE(Contains(legal, DiscardActionForCard(ok)));
    }
  }

  // The cue-bidder may not discard the tarokk they promised (here the XIX).
  {
    std::vector<Action> hand0 = {Tarokk(2), Tarokk(3), Tarokk(4),
                                 Tarokk(5), Tarokk(6), Tarokk(7),
                                 Tarokk(8), kCardXX,   kCardXIX};
    std::vector<Action> talon = UnusedTalon(hand0);
    TalonExchangeState t(DealWith(hand0, talon), talon, /*declarer=*/0,
                         Bid::kTwo, /*cue_bidder=*/0, CalledCard::kXIX);
    DrawTalon(&t);
    while (t.LegalActions().front() == kActionAnnul) {
      t.ApplyAction(kActionDeclineAnnul);
    }
    SPIEL_CHECK_EQ(t.CurrentPlayer(), 0);
    std::vector<Action> legal = t.LegalActions();
    SPIEL_CHECK_FALSE(Contains(legal, DiscardActionForCard(kCardXIX)));
    SPIEL_CHECK_FALSE(Contains(legal, DiscardActionForCard(kCardXX)));
    SPIEL_CHECK_TRUE(Contains(legal, DiscardActionForCard(Tarokk(2))));
  }

  // Annulment with all four kings: P0 (declarer, solo -> draws nothing) is
  // offered the throw-in first and takes it.
  {
    std::vector<Action> hand0 = {
        Tarokk(2),           Tarokk(3),        Tarokk(4),
        Tarokk(5),           Tarokk(6),        MakeKing(kHearts),
        MakeKing(kDiamonds), MakeKing(kClubs), MakeKing(kSpades)};
    std::vector<Action> talon = UnusedTalon(hand0);
    TalonExchangeState t(DealWith(hand0, talon), talon, /*declarer=*/0,
                         Bid::kSolo, kInvalidPlayer, CalledCard::kNone);
    DrawTalon(&t);
    SPIEL_CHECK_EQ(t.CurrentPlayer(), 0);
    std::vector<Action> expected = {kActionAnnul, kActionDeclineAnnul};
    SPIEL_CHECK_EQ(expected, t.LegalActions());
    t.ApplyAction(kActionAnnul);
    SPIEL_CHECK_TRUE(t.IsFinished());
    SPIEL_CHECK_TRUE(t.Annulled());
  }

  // Annulment with the XXI + pagát only: likewise offered and taken.
  {
    std::vector<Action> hand0 = {kCardPagat,
                                 kCardXXI,
                                 SuitCard(kHearts, kLow),
                                 SuitCard(kHearts, kJack),
                                 SuitCard(kHearts, kRider),
                                 SuitCard(kHearts, kQueen),
                                 SuitCard(kDiamonds, kLow),
                                 SuitCard(kDiamonds, kKing),
                                 SuitCard(kDiamonds, kRider)};
    std::vector<Action> talon = UnusedTalon(hand0);
    TalonExchangeState t(DealWith(hand0, talon), talon, /*declarer=*/0,
                         Bid::kSolo, kInvalidPlayer, CalledCard::kNone);
    DrawTalon(&t);
    SPIEL_CHECK_EQ(t.CurrentPlayer(), 0);
    std::vector<Action> expected = {kActionAnnul, kActionDeclineAnnul};
    SPIEL_CHECK_EQ(expected, t.LegalActions());
    t.ApplyAction(kActionAnnul);
    SPIEL_CHECK_TRUE(t.IsFinished());
    SPIEL_CHECK_TRUE(t.Annulled());
  }
}

}  // namespace
}  // namespace hungarian_tarokk
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarokk::BiddingLogicTest();
  open_spiel::hungarian_tarokk::CardHelperTest();
  open_spiel::hungarian_tarokk::TalonLogicTest();
  open_spiel::hungarian_tarokk::BasicHungarianTarokkTests();
}
