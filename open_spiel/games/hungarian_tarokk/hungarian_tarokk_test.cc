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

  // An accepted invite by a cue-bidder whose only honour is the pagát obliges
  // pagátultimó (C §5.2.2): P0 invites with the XIX but P2 wins and calls it
  // in.
  {
    std::vector<PlayerBidInfo> info =
        FourHonours();  // each holds only the pagát
    info[0].has_xix = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidTwo);   // P0 cue bids the XIX
    b.ApplyAction(kActionBidOne);   // P1 overcalls
    b.ApplyAction(kActionBidSolo);  // P2 overcalls
    b.ApplyAction(kActionPass);     // P3 passes
    b.ApplyAction(kActionPass);     // P1 passes; P2 wins (accepts the invite)
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 2);
    SPIEL_CHECK_EQ(b.CueBidder(), 0);
    SPIEL_CHECK_TRUE(b.PagatUltiObligation());
  }

  // The cue-bidder winning its own invite carries no obligation: with nobody to
  // accept it, P0 takes the contract and owes no pagátultimó.
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_xix = true;
    BiddingState b(info);
    b.ApplyAction(kActionBidTwo);  // P0 cue bids the XIX
    b.ApplyAction(kActionPass);    // P1
    b.ApplyAction(kActionPass);    // P2
    b.ApplyAction(kActionPass);    // P3 -> P0 wins its own invite
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_EQ(b.Declarer(), 0);
    SPIEL_CHECK_EQ(b.CueBidder(), 0);
    SPIEL_CHECK_EQ(CalledCard::kNone, b.ObligatoryCalledCard());
    SPIEL_CHECK_FALSE(b.PagatUltiObligation());
  }

  // An accepted invite carries no obligation if the cue-bidder also has a high
  // honour.
  {
    std::vector<PlayerBidInfo> info = FourHonours();
    info[0].has_xix = true;
    info[0].has_high_honour = true;  // P0 also holds the XXI or the Skíz
    BiddingState b(info);
    b.ApplyAction(kActionBidTwo);   // P0 cue bids the XIX
    b.ApplyAction(kActionBidOne);   // P1 overcalls
    b.ApplyAction(kActionBidSolo);  // P2 overcalls
    b.ApplyAction(kActionPass);     // P3
    b.ApplyAction(kActionPass);     // P1 passes; P2 wins
    SPIEL_CHECK_EQ(b.Declarer(), 2);
    SPIEL_CHECK_FALSE(b.PagatUltiObligation());
  }

  // A plain (non-cue) bid never obliges pagátultimó.
  {
    BiddingState b(FourHonours());
    b.ApplyAction(kActionBidThree);  // plain three
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);
    b.ApplyAction(kActionPass);  // P0 keeps the three and wins
    SPIEL_CHECK_TRUE(b.IsFinished());
    SPIEL_CHECK_FALSE(b.PagatUltiObligation());
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
std::vector<std::vector<Card>> DealWith(const std::vector<Card>& hand0,
                                        const std::vector<Card>& talon) {
  std::vector<bool> used(kNumCards, false);
  for (Card c : hand0) used[c.index] = true;
  for (Card c : talon) used[c.index] = true;
  std::vector<std::vector<Card>> hands(kNumPlayers);
  hands[0] = hand0;
  int p = 1;
  for (int c = 0; c < kNumCards; ++c) {
    if (used[c]) continue;
    if (static_cast<int>(hands[p].size()) == kHandSize) ++p;
    hands[p].push_back(Card{c});
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
std::vector<Card> UnusedTalon(const std::vector<Card>& hand) {
  std::vector<bool> used(kNumCards, false);
  for (Card c : hand) used[c.index] = true;
  std::vector<Card> talon;
  for (int c = 0; c < kNumCards && talon.size() < kTalonSize; ++c) {
    if (!used[c]) talon.push_back(Card{c});
  }
  return talon;
}

// Engineered talon exchanges exercising discard legality and annulment.
void TalonLogicTest() {
  // Discard legality: P0 (declarer, "three") holds the pagát/XXI/Skíz, the XX
  // and a king among ordinary tarokks; after drawing it may discard none of
  // those, but may discard the ordinary cards.
  {
    std::vector<Card> hand0 = {kCardPagat, Tarokk(2), Tarokk(3),
                               Tarokk(4),  Tarokk(5), kCardXX,
                               kCardXXI,   kCardSkiz, MakeKing(kHearts)};
    std::vector<Card> talon = UnusedTalon(hand0);
    TalonExchangeState t(DealWith(hand0, talon), talon, /*declarer=*/0,
                         Bid::kThree, kInvalidPlayer, CalledCard::kNone);
    DrawTalon(&t);
    while (t.LegalActions().front() == kActionAnnul) {
      t.ApplyAction(kActionDeclineAnnul);  // skip any opponent's throw-in
    }
    SPIEL_CHECK_EQ(t.CurrentPlayer(), 0);
    std::vector<Action> legal = t.LegalActions();
    std::vector<Card> forbidden = {kCardPagat, kCardXXI, kCardSkiz, kCardXX,
                                   MakeKing(kHearts)};
    for (Card f : forbidden) {
      SPIEL_CHECK_FALSE(Contains(legal, DiscardActionForCard(f)));
    }
    for (Card ok : {Tarokk(2), Tarokk(3), Tarokk(4), Tarokk(5)}) {
      SPIEL_CHECK_TRUE(Contains(legal, DiscardActionForCard(ok)));
    }
  }

  // The cue-bidder may not discard the tarokk they promised (here the XIX).
  {
    std::vector<Card> hand0 = {Tarokk(2), Tarokk(3), Tarokk(4),
                               Tarokk(5), Tarokk(6), Tarokk(7),
                               Tarokk(8), kCardXX,   kCardXIX};
    std::vector<Card> talon = UnusedTalon(hand0);
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
    std::vector<Card> hand0 = {
        Tarokk(2),           Tarokk(3),        Tarokk(4),
        Tarokk(5),           Tarokk(6),        MakeKing(kHearts),
        MakeKing(kDiamonds), MakeKing(kClubs), MakeKing(kSpades)};
    std::vector<Card> talon = UnusedTalon(hand0);
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
    std::vector<Card> hand0 = {kCardPagat,
                               kCardXXI,
                               SuitCard(kHearts, kLow),
                               SuitCard(kHearts, kJack),
                               SuitCard(kHearts, kRider),
                               SuitCard(kHearts, kQueen),
                               SuitCard(kDiamonds, kLow),
                               SuitCard(kDiamonds, kKing),
                               SuitCard(kDiamonds, kRider)};
    std::vector<Card> talon = UnusedTalon(hand0);
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

// Engineered announcement rounds: partner calling and the kontra alternation.
void AnnouncementLogicTest() {
  // A few hands; only the declarer's hand (legal calls) and who holds the
  // called card (the partner) matter.
  auto hands = [](Player xx_holder) {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {Tarokk(2), Tarokk(3), Tarokk(4), Tarokk(5),
            Tarokk(6), Tarokk(7), Tarokk(8), kCardSkiz};  // declarer, no XX
    h[1] = {SuitCard(kHearts, kLow)};
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    h[xx_holder].push_back(kCardXX);
    return h;
  };

  // The declarer lacks the XX, so must call it; the partner is its holder (P2).
  {
    AnnouncementState a(hands(2), /*declarer=*/0, Bid::kThree,
                        CalledCard::kNone);
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 0);
    std::vector<Action> expected = {CallActionForTarokk(kCardXX)};
    SPIEL_CHECK_EQ(expected, a.LegalActions());
    a.ApplyAction(CallActionForTarokk(kCardXX));
    SPIEL_CHECK_EQ(a.Partner(), 2);
  }

  // A cue obligation forces the call (here the XIX).
  {
    AnnouncementState a(hands(2), 0, Bid::kSolo, CalledCard::kXIX);
    std::vector<Action> expected = {CallActionForTarokk(kCardXIX)};
    SPIEL_CHECK_EQ(expected, a.LegalActions());
  }

  // The declarer holds the XX and XIX: it may call the highest tarokk below the
  // XX that it does not hold (the XVIII here), or its own XX to play alone.
  {
    std::vector<std::vector<Card>> h = hands(0);
    h[0].push_back(kCardXIX);
    AnnouncementState a(h, 0, Bid::kThree, CalledCard::kNone);
    std::vector<Action> expected = {CallActionForTarokk(kCardXVIII),
                                    CallActionForTarokk(kCardXX)};
    SPIEL_CHECK_EQ(expected, a.LegalActions());
    a.ApplyAction(CallActionForTarokk(kCardXX));  // call own XX -> play alone
    SPIEL_CHECK_EQ(a.Partner(), kInvalidPlayer);
  }

  // Both sides may announce the same bonus, each with its own kontra chain
  // (§5.2, §5.3). The declarer (side 0) announces four kings; a defender
  // (side 1) must first reveal its side with a kontra before it may announce
  // its own four kings (§5.5); the partner (side 0) may then rekontra its own
  // and kontra the defenders'.
  {
    AnnouncementState a(hands(2), 0, Bid::kThree, CalledCard::kNone);
    a.ApplyAction(CallActionForTarokk(kCardXX));  // P0 calls (partner P2)
    a.ApplyAction(
        AnnounceBonusAction(Bonus::kFourKings));  // P0: (four kings, declarers)
    a.ApplyAction(kActionAnnouncePass);           // P0 ends its turn
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 1);         // a defender (side 1)
    std::vector<Action> legal = a.LegalActions();
    // The defender may kontra the declarers' four kings and the game, but not
    // yet announce its own four kings (its side is not public, and the
    // convention would attribute the announcement to the declarer's side).
    SPIEL_CHECK_TRUE(
        Contains(legal, KontraClaimAction(Bonus::kFourKings, Side::kDeclarers)));
    SPIEL_CHECK_TRUE(Contains(legal, kKontraActionBase + kGameKontraItem));
    SPIEL_CHECK_FALSE(Contains(legal, AnnounceBonusAction(Bonus::kFourKings)));
    // Kontra-ing reveals P1 as a defender; now it may announce its own bonus.
    a.ApplyAction(
        KontraClaimAction(Bonus::kFourKings, Side::kDeclarers));  // kontra P0's
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDefenders);
    SPIEL_CHECK_TRUE(
        Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kFourKings)));
    a.ApplyAction(
        AnnounceBonusAction(Bonus::kFourKings));  // P1: (four kings, defenders)
    a.ApplyAction(kActionAnnouncePass);
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 2);  // declarer's partner (side 0)
    std::vector<Action> legal2 = a.LegalActions();
    // The declarers' four kings (now level 1) is theirs to rekontra; the
    // defenders' (level 0) is theirs to kontra -- two distinct, unambiguous
    // actions.
    SPIEL_CHECK_TRUE(Contains(
        legal2, KontraClaimAction(Bonus::kFourKings, Side::kDeclarers)));
    SPIEL_CHECK_TRUE(Contains(
        legal2, KontraClaimAction(Bonus::kFourKings, Side::kDefenders)));
    // Rekontra-ing reveals P2 as the partner; P3 is then a defender by
    // elimination (§5.5).
    a.ApplyAction(KontraClaimAction(Bonus::kFourKings, Side::kDeclarers));
    SPIEL_CHECK_TRUE(a.PublicSide(2) == Side::kDeclarers);
    SPIEL_CHECK_TRUE(a.PublicSide(3) == Side::kDefenders);
  }
}

// The mandatory pagátultimó (C §5.2.2) and the 8/9 tarokk declaration (§5.4).
void MandatoryAnnouncementTest() {
  // The obliged cue-bidder may be the partner; the obligation fires on its own
  // turn (and it holds too few tarokks to declare, so only the ulti is owed).
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {SuitCard(kHearts, kLow)};  // declarer (forced to call the XX)
    h[1] = {SuitCard(kHearts, kJack)};
    h[2] = {kCardXX, kCardPagat, Tarokk(2)};  // partner; only 3 tarokks
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, /*declarer=*/0, Bid::kTwo, CalledCard::kXX,
                        /*pagat_ulti_player=*/2);
    a.ApplyAction(
        CallActionForTarokk(kCardXX));  // declarer calls -> partner P2
    SPIEL_CHECK_EQ(a.Partner(), 2);
    a.ApplyAction(kActionAnnouncePass);  // declarer (not obliged) passes
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 1);
    SPIEL_CHECK_TRUE(
        Contains(a.LegalActions(), kActionAnnouncePass));  // P1 free
    a.ApplyAction(kActionAnnouncePass);                    // defender P1 passes
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 2);
    std::vector<Action> legal = a.LegalActions();  // P2 obliged
    SPIEL_CHECK_TRUE(Contains(legal, AnnounceBonusAction(Bonus::kPagatUlti)));
    SPIEL_CHECK_FALSE(Contains(legal, kActionAnnouncePass));
    a.ApplyAction(AnnounceBonusAction(Bonus::kPagatUlti));
    SPIEL_CHECK_TRUE(
        Contains(a.LegalActions(), kActionAnnouncePass));  // <8 tarokks
  }

  // A defender that kontras a pagátultimó must also declare its 8/9 (C §4.1.3);
  // here it holds eight tarokks, so eight is owed (and nine is never offered).
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX};  // declarer
    h[1] = {Tarokk(1), Tarokk(2), Tarokk(3), Tarokk(4),
            Tarokk(5), Tarokk(6), Tarokk(7), Tarokk(8)};  // defender, 8 tarokks
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, /*declarer=*/0, Bid::kThree, CalledCard::kNone);
    a.ApplyAction(CallActionForTarokk(kCardXX));  // play alone (own XX)
    SPIEL_CHECK_EQ(a.Partner(), kInvalidPlayer);
    a.ApplyAction(
        AnnounceBonusAction(Bonus::kPagatUlti));  // declarer announces
    a.ApplyAction(kActionAnnouncePass);
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 1);  // the defender
    a.ApplyAction(KontraClaimAction(Bonus::kPagatUlti, Side::kDeclarers));
    std::vector<Action> legal = a.LegalActions();
    SPIEL_CHECK_TRUE(
        Contains(legal, kActionDeclareEight));  // must declare eight
    SPIEL_CHECK_FALSE(Contains(legal, kActionDeclareNine));
    SPIEL_CHECK_FALSE(
        Contains(legal, kActionAnnouncePass));  // may not pass yet
    a.ApplyAction(kActionDeclareEight);
    SPIEL_CHECK_EQ(a.DeclaredTarokks(1), 8);
    SPIEL_CHECK_TRUE(
        Contains(a.LegalActions(), kActionAnnouncePass));  // free now
  }
}

// Public side-deduction from the call and the announcements (§5.5).
void PublicSideDeductionTest() {
  // After an accepted cue bid (a forced call) the whole table is public from the
  // start: the inviter is the publicly-known partner.
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {SuitCard(kHearts, kLow)};  // declarer
    h[1] = {kCardXIX};                 // the cue-bidder/partner holds the XIX
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, /*declarer=*/0, Bid::kThree, CalledCard::kXIX);
    a.ApplyAction(CallActionForTarokk(kCardXIX));  // forced call of the cued card
    SPIEL_CHECK_EQ(a.Partner(), 1);
    SPIEL_CHECK_TRUE(a.PublicSide(0) == Side::kDeclarers);
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDeclarers);
    SPIEL_CHECK_TRUE(a.PublicSide(2) == Side::kDefenders);
    SPIEL_CHECK_TRUE(a.PublicSide(3) == Side::kDefenders);
  }

  // Free call of a non-XX tarokk: a partner must exist (you may call yourself
  // only with the XX), so once both defenders reveal themselves the hidden
  // partner is deducible by elimination.
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX};                  // declarer holds the XX
    h[1] = {SuitCard(kHearts, kLow)};
    h[2] = {kCardXIX};                 // the partner holds the called XIX
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, /*declarer=*/0, Bid::kThree, CalledCard::kNone);
    a.ApplyAction(CallActionForTarokk(kCardXIX));  // non-XX call -> partner exists
    SPIEL_CHECK_EQ(a.Partner(), 2);
    // Initially only the declarer's side is public.
    SPIEL_CHECK_TRUE(a.PublicSide(0) == Side::kDeclarers);
    SPIEL_CHECK_FALSE(a.PublicSide(1).has_value());
    SPIEL_CHECK_FALSE(a.PublicSide(2).has_value());
    SPIEL_CHECK_FALSE(a.PublicSide(3).has_value());
    a.ApplyAction(kActionAnnouncePass);                  // P0 passes after calling
    a.ApplyAction(kKontraActionBase + kGameKontraItem);  // P1 kontras -> defender
    a.ApplyAction(kActionAnnouncePass);                  // P1 passes
    a.ApplyAction(kActionAnnouncePass);                  // P2 (partner) passes
    a.ApplyAction(AnnounceBonusAction(Bonus::kFourKings));  // P3 reveals: defender
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDefenders);
    SPIEL_CHECK_TRUE(a.PublicSide(3) == Side::kDefenders);
    SPIEL_CHECK_TRUE(a.PublicSide(2) == Side::kDeclarers);  // by elimination
  }

  // A bare XX call may instead be a lone declarer, so revealing the same two
  // defenders does NOT pin the third as a partner.
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {SuitCard(kHearts, kLow)};  // declarer lacks the XX -> must call it
    h[1] = {SuitCard(kHearts, kJack)};
    h[2] = {kCardXX};                  // the actual partner holds the XX
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, /*declarer=*/0, Bid::kThree, CalledCard::kNone);
    a.ApplyAction(CallActionForTarokk(kCardXX));         // bare XX call
    SPIEL_CHECK_EQ(a.Partner(), 2);
    a.ApplyAction(kActionAnnouncePass);                  // P0 passes
    a.ApplyAction(kKontraActionBase + kGameKontraItem);  // P1 kontras -> defender
    a.ApplyAction(kActionAnnouncePass);                  // P1 passes
    a.ApplyAction(kActionAnnouncePass);                  // P2 passes
    a.ApplyAction(AnnounceBonusAction(Bonus::kFourKings));  // P3 reveals: defender
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDefenders);
    SPIEL_CHECK_TRUE(a.PublicSide(3) == Side::kDefenders);
    // P2 stays hidden: the declarer might be playing alone.
    SPIEL_CHECK_FALSE(a.PublicSide(2).has_value());
  }
}

// Calling a tarokk a defender discarded (§4.3, §6.5) and the automatic
// "hivatalból kontra".
void DiscardedTarokkCallTest() {
  // The declarer holds the XX and defender P2 laid an ordinary tarokk away. Under
  // §6.5 the declarer may call that discarded tarokk; doing so makes it play
  // alone and forces P2's by-office kontra of the game.
  {
    std::vector<std::vector<Card>> hands(kNumPlayers);
    hands[0] = {kCardXX, Tarokk(2), Tarokk(3)};  // declarer holds the XX
    hands[1] = {SuitCard(kHearts, kLow)};
    hands[2] = {SuitCard(kHearts, kJack)};
    hands[3] = {SuitCard(kHearts, kRider)};
    std::vector<std::vector<Card>> discards(kNumPlayers);
    discards[2] = {Tarokk(5)};  // P2 discarded the V (an ordinary tarokk)

    AnnouncementState a(hands, /*declarer=*/0, Bid::kThree, CalledCard::kNone,
                        /*pagat_ulti_player=*/kInvalidPlayer, /*num_bidders=*/0,
                        discards);
    std::vector<Action> legal = a.LegalActions();
    // The §6.5 free call offers the discarded V and the declarer's own XX, but
    // never an honour or a card the declarer holds.
    SPIEL_CHECK_TRUE(Contains(legal, CallActionForTarokk(Tarokk(5))));
    SPIEL_CHECK_TRUE(Contains(legal, CallActionForTarokk(kCardXX)));
    SPIEL_CHECK_FALSE(Contains(legal, CallActionForTarokk(kCardPagat)));  // honour
    SPIEL_CHECK_FALSE(Contains(legal, CallActionForTarokk(Tarokk(2))));   // in hand

    a.ApplyAction(CallActionForTarokk(Tarokk(5)));  // call the discarded tarokk
    SPIEL_CHECK_EQ(a.Partner(), kInvalidPlayer);          // declarer plays alone
    SPIEL_CHECK_EQ(a.HivatalbolKontraPlayer(), 2);        // the discarder, P2
    // The whole table is public: the lone declarer against three defenders.
    SPIEL_CHECK_TRUE(a.PublicSide(0) == Side::kDeclarers);
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDefenders);
    SPIEL_CHECK_TRUE(a.PublicSide(2) == Side::kDefenders);
    SPIEL_CHECK_TRUE(a.PublicSide(3) == Side::kDefenders);
    // The game is already kontra'd by office, so the declarer's side may rekontra
    // it (and no defender may kontra it again).
    SPIEL_CHECK_TRUE(
        Contains(a.LegalActions(), kKontraActionBase + kGameKontraItem));
  }

  // Calling a tarokk a defender holds in hand (not discarded) is an ordinary
  // partner call -- no play-alone, no hivatalból kontra.
  {
    std::vector<std::vector<Card>> hands(kNumPlayers);
    hands[0] = {kCardXX, Tarokk(2), Tarokk(3)};
    hands[1] = {SuitCard(kHearts, kLow)};
    hands[2] = {Tarokk(7)};  // P2 holds the VII in hand
    hands[3] = {SuitCard(kHearts, kRider)};
    std::vector<std::vector<Card>> discards(kNumPlayers);
    discards[1] = {Tarokk(5)};  // some defender discarded a tarokk (enables §6.5)

    AnnouncementState a(hands, 0, Bid::kThree, CalledCard::kNone, kInvalidPlayer,
                        /*num_bidders=*/0, discards);
    SPIEL_CHECK_TRUE(Contains(a.LegalActions(), CallActionForTarokk(Tarokk(7))));
    a.ApplyAction(CallActionForTarokk(Tarokk(7)));  // P2 holds it -> partner
    SPIEL_CHECK_EQ(a.Partner(), 2);
    SPIEL_CHECK_EQ(a.HivatalbolKontraPlayer(), kInvalidPlayer);
  }
}

// The §5.7 honour promises that gate announcing trull
void TrullPromiseTest() {
  // Whether the declarer (P0), holding the XX plus the given honours, may
  // announce trull immediately
  auto declarer_trull = [](std::vector<Card> declarer_honours, int num_bidders) {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX};
    for (Card c : declarer_honours) h[0].push_back(c);
    h[1] = {SuitCard(kHearts, kLow), kCardXIX};
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, /*declarer=*/0, Bid::kThree, CalledCard::kNone,
                        kInvalidPlayer, num_bidders);
    a.ApplyAction(CallActionForTarokk(kCardXIX));
    return Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull));
  };

  // C §4.2.1 -- simple game, fewer than three bidders: the declarer's first-round
  // trull promises BOTH high honours (Skíz and XXI).
  SPIEL_CHECK_TRUE(declarer_trull({kCardSkiz, kCardXXI}, 2));
  SPIEL_CHECK_FALSE(declarer_trull({kCardSkiz}, 2));
  SPIEL_CHECK_FALSE(declarer_trull({kCardXXI}, 2));
  SPIEL_CHECK_FALSE(declarer_trull({}, 2));

  // C §4.2.1 -- simple game, three bidders: only the Skíz is promised (the
  // honours are split one-per-bidder, so both high honours cannot be guaranteed).
  SPIEL_CHECK_TRUE(declarer_trull({kCardSkiz}, 3));
  SPIEL_CHECK_TRUE(declarer_trull({kCardSkiz, kCardXXI}, 3));
  SPIEL_CHECK_FALSE(declarer_trull({kCardXXI}, 3));    // must be the Skíz itself
  SPIEL_CHECK_FALSE(declarer_trull({kCardPagat}, 3));  // pagát is not high

  // Whether the declarer (P0), holding the given honours, may announce trull in
  // its first round of an INVITED game (P1 holds the cued XIX, the partner).
  auto invited_declarer_trull = [](std::vector<Card> declarer_honours) {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {Tarokk(2)};  // declarer; forced to call the XIX
    for (Card c : declarer_honours) h[0].push_back(c);
    h[1] = {kCardXX};  // partner
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, 0, Bid::kThree, CalledCard::kXIX, kInvalidPlayer, 2);
    a.ApplyAction(CallActionForTarokk(kCardXIX));  // forced call of the cued card
    return Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull));
  };

  // C §4.2.2 -- an invited (or yielded) game only promises >= 1 high honour.
  SPIEL_CHECK_TRUE(invited_declarer_trull({kCardSkiz}));
  SPIEL_CHECK_TRUE(invited_declarer_trull({kCardXXI}));
  SPIEL_CHECK_FALSE(invited_declarer_trull({kCardPagat}));  // only a low honour
  SPIEL_CHECK_FALSE(invited_declarer_trull({}));

  // Whether the simple-game partner (P2, holder of the called XIX) may announce
  // trull on its first turn, holding the given honours.
  auto partner_trull = [](std::vector<Card> partner_honours) {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX};  // declarer -> calls the XIX (highest tarokk below the XX)
    h[1] = {SuitCard(kHearts, kLow)};
    h[2] = {kCardXIX};  // the partner holds the called card
    for (Card c : partner_honours) h[2].push_back(c);
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, 0, Bid::kThree, CalledCard::kNone, kInvalidPlayer, 2);
    a.ApplyAction(CallActionForTarokk(kCardXIX));  // simple call of the XIX
    SPIEL_CHECK_EQ(a.Partner(), 2);
    a.ApplyAction(kActionAnnouncePass);    // declarer passes, no announcement
    a.ApplyAction(kActionAnnouncePass);    // defender P1 passes
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 2);  // the partner, on its first turn
    return Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull));
  };

  // C §4.2.3 -- a simple game's called partner promises >= 1 high honour.
  SPIEL_CHECK_TRUE(partner_trull({kCardSkiz}));
  SPIEL_CHECK_TRUE(partner_trull({kCardXXI}));
  SPIEL_CHECK_FALSE(partner_trull({kCardPagat}));
  SPIEL_CHECK_FALSE(partner_trull({}));

  // Whether the yielder/partner (P1, holding the XX plus `partner_honours`) may
  // pick up the trull on its first turn after the declarer declined it.
  auto pickup_trull = [](std::vector<Card> partner_honours) {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {SuitCard(kHearts, kLow)};  // declarer -> forced to call the XX
    h[1] = partner_honours;
    h[1].push_back(kCardXX);           // the yielder holds the XX
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, 0, Bid::kTwo, CalledCard::kXX, kInvalidPlayer, 2);
    a.ApplyAction(CallActionForTarokk(kCardXX));  // forced call of the XX
    SPIEL_CHECK_EQ(a.Partner(), 1);
    a.ApplyAction(kActionAnnouncePass);    // declarer passes, no trull announced
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 1);  // the yielder/partner, first turn
    return Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull));
  };

  // C §4.2.2 (Megjegyzés) -- the inviter/yielder picking up a declined trull
  // promises TWO of the three honours, so a single high honour is not enough.
  SPIEL_CHECK_TRUE(pickup_trull({kCardSkiz, kCardPagat}));
  SPIEL_CHECK_TRUE(pickup_trull({kCardSkiz, kCardXXI}));
  SPIEL_CHECK_FALSE(pickup_trull({kCardSkiz}));   // one honour only
  SPIEL_CHECK_FALSE(pickup_trull({kCardPagat}));  // one honour only

  // C §4.2.4 / §5.7 -- a defender's trull needs >= 1 high honour AND the defender
  // must reveal its side first. Play-alone game: P0 calls its own XX.
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX};                            // declarer plays alone
    h[1] = {kCardXXI, SuitCard(kHearts, kLow)};  // a defender with a high honour
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, 0, Bid::kThree, CalledCard::kNone, kInvalidPlayer, 2);
    a.ApplyAction(CallActionForTarokk(kCardXX));  // play alone
    a.ApplyAction(kActionAnnouncePass);           // declarer passes
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 1);
    // P1's side is not yet public, so it may not announce trull for its own side
    // until it reveals with a kontra.
    SPIEL_CHECK_FALSE(
        Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull)));
    a.ApplyAction(kKontraActionBase + kGameKontraItem);  // reveal as a defender
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDefenders);
    // Holding the XXI, it may now announce its trull.
    SPIEL_CHECK_TRUE(
        Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull)));
  }
  // A defender with no high honour cannot announce trull even after revealing.
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX};
    h[1] = {Tarokk(5), SuitCard(kHearts, kLow)};  // defender, no high honour
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, 0, Bid::kThree, CalledCard::kNone, kInvalidPlayer, 2);
    a.ApplyAction(CallActionForTarokk(kCardXX));
    a.ApplyAction(kActionAnnouncePass);
    a.ApplyAction(kKontraActionBase + kGameKontraItem);  // reveal as a defender
    SPIEL_CHECK_TRUE(a.PublicSide(1) == Side::kDefenders);
    SPIEL_CHECK_FALSE(
        Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull)));
  }

  // The strict first-round promise relaxes to the second-round rule (>= 1 high
  // honour, C §4.2.4) on a later turn: a declarer holding only the Skíz cannot
  // announce trull in the first round of a simple <3-bidder game (both high
  // honours required) but may once it comes back around.
  {
    std::vector<std::vector<Card>> h(kNumPlayers);
    h[0] = {kCardXX, kCardSkiz};  // declarer, one high honour, plays alone
    h[1] = {SuitCard(kHearts, kLow)};
    h[2] = {SuitCard(kHearts, kJack)};
    h[3] = {SuitCard(kHearts, kRider)};
    AnnouncementState a(h, 0, Bid::kThree, CalledCard::kNone, kInvalidPlayer, 2);
    a.ApplyAction(CallActionForTarokk(kCardXX));  // play alone
    SPIEL_CHECK_FALSE(  // first round: only the Skíz -> refused
        Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull)));
    a.ApplyAction(kActionAnnouncePass);                  // P0 ends its first turn
    a.ApplyAction(kKontraActionBase + kGameKontraItem);  // P1 keeps the phase alive
    a.ApplyAction(kActionAnnouncePass);                  // P1
    a.ApplyAction(kActionAnnouncePass);                  // P2
    a.ApplyAction(kActionAnnouncePass);                  // P3
    SPIEL_CHECK_EQ(a.CurrentPlayer(), 0);  // back to the declarer, second round
    SPIEL_CHECK_TRUE(  // second round: >= 1 high honour suffices
        Contains(a.LegalActions(), AnnounceBonusAction(Bonus::kTrull)));
  }
}

}  // namespace
}  // namespace hungarian_tarokk
}  // namespace open_spiel

int main(int argc, char** argv) {
  open_spiel::hungarian_tarokk::BiddingLogicTest();
  open_spiel::hungarian_tarokk::CardHelperTest();
  open_spiel::hungarian_tarokk::TalonLogicTest();
  open_spiel::hungarian_tarokk::AnnouncementLogicTest();
  open_spiel::hungarian_tarokk::MandatoryAnnouncementTest();
  open_spiel::hungarian_tarokk::PublicSideDeductionTest();
  open_spiel::hungarian_tarokk::DiscardedTarokkCallTest();
  open_spiel::hungarian_tarokk::TrullPromiseTest();
  open_spiel::hungarian_tarokk::BasicHungarianTarokkTests();
}
