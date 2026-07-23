# Hungarian Tarokk (Paskievics / XX-hívásos) — Rules to Implement

This document is the implementation specification for the **Paskievics** (also
*Húszashívásos* / "XX-calling") variant of Hungarian Tarokk, as actually played
under the Hungarian tournament rules.

It is synthesized from two sources in this directory:

- **`rules.html`** — the general pagat.com description by John McLeod
  (the *base* rules and all variant options).
- **`competition-rules.txt`** — *"Paskievics tarokkversenyeken alkalmazandó
  versenyszabályok"*, effective 2018‑03‑01 (the *specific variant* we implement;
  cited below as **C §x**).

Where the competition rules pin down one of pagat's optional variants, **the
competition rule wins** and the pagat alternatives are dropped. Items that are
genuinely ambiguous are tagged inline with **[UNCLEAR]** and collected in the
[Open Questions](#open-questions--unclear-items) section.

---

## 0. Scope and engine model

The OpenSpiel game models **one deal** (one hand) as a sequential game.

In scope (one deal):

- Dealing, bidding, talon exchange + discard, calling a partner, the round of
  announcements, the 9-trick play, and the final scoring of that deal.

Out of scope (session / tournament level — document only, do not implement in
the per-deal game logic):

- Seating randomization and the "mayor's hat" (pagat §*Choice of seats*).
- Session end ("A skíz oszt, nem oszt").
- Tournament structure: 4 rounds, 16 deals per round, 80-minute round cap,
  seeding (**C §2**), and competition points 10‑8‑6‑4 (**C §3**).
- The doubled-round mechanic after a passed-out / annulled hand (pagat). The
  competition document does **not** mention it (a tournament plays a fixed
  number of deals), so for a single-deal engine a passed-out / annulled hand
  simply produces **no score** (terminal, all-zero returns) or a redeal,
  per the harness's choice. This implementation should simply produce all-zero
  returns and end the game.

**Players.** Exactly **4 players**, all active in every deal.

**Direction of play.** Everything proceeds **anticlockwise**. The player to the
**dealer's right** ("forehand") bids first and leads to the first trick. The turn
to deal passes to the right after each deal.

---

## 1. Cards and card points

42-card pack (a 54-card Tarock pack with the red 2/3/4 and black 7/8/9 removed).

### 1.1 Trumps (tarokks), 22 cards, high → low

```
Skíz  (highest)
XXI
XX  XIX  XVIII  XVII  XVI  XV  XIV  XIII  XII  XI  X  IX  VIII  VII  VI  V  IV  III  II
I  (= "pagát", lowest)
```

- **Honours** (*honőrök*): **Skíz, XXI, I (pagát)** — 5 card points each.
- **High honours** (*nagyhonőrök*): **Skíz** and **XXI**.
- All other tarokks (II–XX, 19 cards): 1 card point each.

### 1.2 Suits — hearts, diamonds, clubs, spades, 5 cards each, high → low

`King > Queen > Rider (Cavall) > Jack > [Ten (black) / Ace (red)]`

(Red suits use the Ace as their lowest card; black suits use the Ten. Some
groups use the red 4 instead of the Ace — cosmetic only, no rule effect.)

### 1.3 Card-point values

| Card | Points |
|---|---|
| King | 5 |
| Queen | 4 |
| Rider (Cavall) | 3 |
| Jack | 2 |
| Ten / Ace (lowest suit card) | 1 |
| Honour (Skíz, XXI, pagát) | 5 |
| Other tarokk (II–XX) | 1 |

**Total in the pack: 94 points** (15 per suit × 4 = 60; honours 15; other
tarokks 19).

---

## 2. The deal

1. Shuffle; player to dealer's left cuts.
2. Deal **6 cards face down → the talon**.
3. Deal **5 cards** to each active player (starting at dealer's right, going
   anticlockwise), then **4 cards** to each in the same way.
4. Result: 9 cards per player, 6 in the talon.

---

## 3. Bidding (the auction)

Bids, low → high, naming the number of talon cards the eventual declarer will
exchange:

| Bid | Hungarian | Talon cards drawn by declarer | Base game value |
|---|---|---|---|
| three | három | 3 | 1 |
| two | kettő | 2 | 2 |
| one | egy | 1 | 3 |
| solo | szóló | 0 | 4 |

### 3.1 Bidding procedure

- Bidding starts at the **forehand** (dealer's right), proceeds anticlockwise.
- **Entry requirement:** to bid at all you must hold **at least one honour**
  (Skíz, XXI, or pagát). All honours are equal for this purpose (**C §5.1.1**).
  Without an honour you must pass — *except* the 4th-seat trial bid (§3.4).
- A pass is final for that player (cannot re-enter the auction).
- Each new bid must be **strictly higher** than the current bid, with one
  exception — **hold** (*tartom*): a player who has already bid may match the
  current highest bid instead of raising.
  - A bid can be **held only once**: if the last positive call was a hold, the
    next player must raise or pass.
  - **Hold eligibility (newer rule — implement this):** any player who has
    already made a positive bid may hold. *(The older rule restricting holds to
    players whose first turn came earlier is a pagat variation we do NOT use; it
    interacts with cue bids. See [Open Questions](#open-questions--unclear-items)
    to confirm.)*
- The auction ends when all but one player have passed, or no higher bid is
  legal. The last bidder is the **declarer** (*felvevő*).
- **All four pass** → hand is thrown in, all players receive 0 points and the game ends.

### 3.2 The sole bidder may raise the value (C §5.1.2)

If exactly one player ends up bidding (everyone else passed at once), that sole
bidder **may freely raise** the contract by choosing to take fewer talon cards
(3 → 2 → 1 → 0), thereby increasing the game value. **Exception:** not if they
opened with a front cue bid that was not accepted, only if they were the only bidder
with a bid of 3.

### 3.3 Conventional bids (cue bids / *invit*) — mandatory

These are part of the rules, not optional table talk: if you make a conventional
bid you **must** hold the promised card.

- A **single jump** above the minimum available bid is a cue bid promising the
  **XIX** (**C §5.2.5**).
- A **double jump** is a cue bid promising the **XVIII** (**C §5.2.5**).
- Purpose: if another player wins the auction, that declarer **must call the
  promised card** (XIX or XVIII) to make the cue-bidder their partner.

Constraints:

- **Only one cue bid per auction.** A second jump after a cue bid has no
  conventional meaning (**C §5.2.4** "no counter-invite": accepting an invite
  with a jump is not a new invite).
- A bid by the **4th player after three passes is never a cue bid** (no one can
  take it over).
- You may **open** the auction with a cue bid (**C §5.2.3**). An opening **solo**
  is never a cue bid (it cannot be accepted).
- You may cue-bid holding **only the pagát** as your honour, but then you
  **must announce pagátultimó** at your first turn in the round of announcements
  (**C §5.2.2**, pagat).
- A cue bid / invite **cannot be taken back** (**C §5.2.6**).
- If the inviter does not win the auction, the winner **must** call the invited
  card; failing to is **RENONSZ** (**C §5.2.1**). If the promised card is not
  actually in the inviter's hand → **RENONSZ** (**C §5.2.7**).
- Inviting with the pagát obliges pagátultimó; the correct announcement is just
  *"pagát ultimó"* — there is no separate "compulsory pagát ultimó" call
  (**C §5.2.2**).

> Convention version: we use the **modern** convention (single jump = XIX,
> double jump = XVIII), per **C §5.2.5**. The older "any jump that can only be
> overcalled at solo = XVIII" convention and the Pais XVII cue bid are **not**
> implemented.

### 3.4 Yielded game (*engedett játék*)

Occurs when one player bid **three**, another bid **two**, and the other two
passed. A subsequent **pass by the original three-bidder** is conventional: it
**guarantees the XX plus at least one high honour** (Skíz or XXI), and the
two-bidder (now declarer) **must call the XX** (**C §5.2.8**, pagat §yielded).

- A yield is a special kind of invite; same rules apply, except the promised
  card is the XX (**C §5.2.8**).
- **C §5.1.3 / §5.2.8:** *"A XX-as pagátos a játékot nem engedheti"* - unlike 
  XIX and XVIII invites, yielding the game with the pagat and no high honour is
  illegal.

### 3.5 Trial bid (*próbahárom* / próba licit) — 4th seat only

If the first three players pass, the **4th player may bid without holding any
honour**, speculating on drawing one from the talon (**C §8.2**, pagat §bidding).

- If the trial bidder **does not** obtain an honour from the talon, the game is
  **lost without play**: the trial bidder pays **3 points to each of the other
  three players** (**C §8.2**; this is the value of a "one" game, but note that
  if an honour is drawn, the value of the game will be 1). 
- This trial bid can only be made at the **three** level.

---

## 4. Talon exchange and discard (*fektetés* / *skart*)

### 4.1 Distributing the talon

Declarer takes their bid's number of cards off the top; the rest go to the other
three players (in anticlockwise order **starting at the declarer's right**), as
equally as possible, the players nearest the declarer's right taking the extra:

| Final bid | Declarer | 2nd player | 3rd player | 4th player |
|---|---|---|---|---|
| three | 3 | 1 | 1 | 1 |
| two | 2 | 2 | 1 | 1 |
| one | 1 | 2 | 2 | 1 |
| solo | 0 | 2 | 2 | 2 |

("2nd/3rd/4th player" = the three non-declarers in anticlockwise order beginning
at the declarer's right.)

### 4.2 Discarding

Each player adds their talon cards and **discards back down to 9**.

- **Declarer's discard** is kept separate and **its card points count for the
  declarer's side.**
- The **other three players' discards** are combined into one pile whose points
  **count for the opponents** (even though one of those three is normally the
  declarer's partner).

**Discard legality (C §6.1):** it is illegal to discard any of:

- any **honour** (Skíz, XXI, pagát),
- any **king**,
- the **XX**,
- a tarokk you have **promised by a cue bid / yield** (the invited card).

Any other card — including ordinary tarokks — may be discarded.

**Announcing discarded tarokks (C §6.3, §6.6):** a player who discards any
tarokk(s) **must announce how many**, *before the round of announcements begins*.
Failure = **RENONSZ**.

**Declarer's discarded tarokks are shown face up** when all discards are
complete, and stay face up until the lead to the first trick (**C §6.4**, pagat).

**Modifying the discard (C §6.7):** the discard may be changed up until the first
announcement; after that it may not even be looked at.

### 4.3 Calling a discarded (laid) tarokk (C §6.4–§6.5)

Because non-declarers may discard ordinary tarokks, a called card can turn out to
be in the discard:

- The player who discarded the called tarokk **must immediately kontra the game**
  — *"Hivatalból kontra"* (kontra by office) — after the declarer's bid is closed
  with pass. In this case **the entire talon/discard counts for the declarer's
  side** (**C §6.4**).
- **C §6.5:** if a tarokk has been discarded **and the declarer holds the XX**,
  the declarer may call **any** tarokk — *except* a tarokk the declarer
  themselves discarded and honours.

### 4.4 Annulling the hand (C §6.2)

After the talon exchange, **before discarding**, a player may **throw in the
hand** (annul, no score) if and only if they hold one of:

- all four **kings**,
- the **XXI** and no other tarokk,
- the **pagát (I)** and no other tarokk,
- the **XXI + pagát** and no other tarokk,
- **no tarokks at all**.

- Annulment is **voluntary**.
- **C §6.2** allows annulment only **before the discard** (*"Lapot bedobni csak
  fektetés előtt"*). This differs from pagat, which lets a four-kings holder
  annul even after discarding a tarokk. **Implement the competition rule:
  annul only before discarding.**
- An annulled hand scores nothing (see §0 for engine handling).

---

## 5. Round of announcements

After all discards, a second "auction" runs, **starting with the declarer** and
proceeding anticlockwise, **for as many rounds as needed until three players in
succession only pass.** At each turn a player may, in this order, then say pass:

1. **Declare 8 or 9 tarokks** (*tarokkszám*).
2. (Declarer's first turn only) **Call the partner.**
3. **Announce one or more bonuses.**
4. **Kontra / rekontra / …** a thing announced by the other team.

Each turn ends with **"pass"** (*passz* / *mehet*).

### 5.1 Calling a partner (declarer's first turn) (pagat §calling-a-partner)

The declarer names a tarokk; its holder is the **partner** (*partner/segítő*) and
stays hidden. The other two are the **opponents/defenders**. Normally the called
card is the **XX**. Exceptions:

- If the declarer **holds the XX**, they instead call the **highest tarokk below
  XX that they do not hold** — *or* may call their own XX to **play alone**
  (three opponents, hidden until revealed).
  - **C §5.1.4:** *"A felvevő önmagát csak XX-al hívhatja"* — the declarer may
    call **themselves only with the XX**. RENONSZ otherwise. (I.e., the
    play-alone option is the own-XX call; you can't engineer a solo by calling
    some other card you hold.)
- If a non-declarer discarded a tarokk → see §4.3 (may call any non-honour).
- If a **cue bid** was made, the declarer **must** call the indicated **XIX/XVIII**
  (not the XX). For a **yielded game**, the declarer **must** call the **XX**.
  This holds even if tarokks were discarded — the promised card cannot be
  discarded and must be called.

### 5.2 Bonuses (*figurák*) — values per competition (C §4)

Bonuses are won/lost by a **partnership**. Each may be made **silently** or
**announced** in advance; announcing doubles the stake (win double / lose
double). Unless noted, bonuses score **independently** of each other and of the
game.

**Announcements and kontras are per-side, not per-player.** Every bonus
announcement and every kontra is made *on behalf of the announcer's team* — the
declarer's side or the defenders' side (which side an announcement belongs to is
fixed by the side-deduction rules of §5.5). A bonus belongs to whichever side
announced it, and **either** side may go for **any** bonus:

- **Both sides may announce the same bonus**, independently of each other. The
  two announcements are scored separately: each announcing side is paid the
  announced amount by the other if it **achieves** the bonus, and pays the other
  if it announced and **failed**. Consequently it is possible for **one** side to
  both lose its own failed announcement and win the bonus it actually made, and
  possible for **neither** side to achieve an announced bonus (both failed
  announcements then pay out). See §7.3 for the worked scoring.
- A side may also achieve a bonus **silently** (unannounced) even though the
  other side announced it; that silent bonus is scored **in addition** to the
  other side's failed announcement.

| Bonus | Silent | Announced | Condition |
|---|---|---|---|
| **Trull** (*tulétroá*) | 1 | 2 | win all three honours (Skíz, XXI, pagát) in tricks |
| **Four kings** (*négykirály*) | 1 | 2 | win all four kings in tricks |
| **Pagátultimó** | 5 | 10 | win the **last** trick with the pagát (the pagát must itself win) |
| **XXI-catch** (*huszonegyfogás*) | 21 | 42 | capture an **opponent's** XXI with the Skíz |
| **Double game** (*dupla / duplajáték*) | ×2 game | ×4 game | take ≥ 71 card points (opponents ≤ 23) |
| **Volát** | ×3 game | ×6 game | win **all nine** tricks |

Notes / competition specifics:

- **Pagátultimó** (**C §4.7**, pagat): winning the **last** trick with the pagát
  (the pagát itself must win it). The **silent** bonus is *not* symmetric — it
  only comes into play if the pagát is actually played to the last trick, and it
  can be **lost even though it was never announced**:
  - pagát **wins** the last trick → silent pagátultimó **made**: **+5** to the
    pagát's side.
  - pagát is **played to the last trick and loses** → **silent failed
    pagátultimó**, even with no announcement: the pagát's side **pays 5** to the
    other side. This holds **even if the pagát's own partner won that last
    trick** (the pagát did not win, so it fails).
  - pagát is **never played to the last trick** (captured earlier, or played to
    an earlier trick) → **no** silent pagátultimó scoring at all — neither +5 nor
    −5. When unannounced there is **no obligation** to keep the pagát back, so
    this is the normal "do nothing" outcome.
  - **Announced** pagátultimó pays **±10**. The holder is then **obliged to keep
    the pagát as late as legally possible** (may only play it when forced by the
    follow-suit / must-trump rules), even once it can no longer win. It fails
    (−10) if the pagát is beaten in the last trick, forced out before the last
    trick, or (rare) the announcing team does not hold the pagát at all.
- **XXI-catch** (**C §4.4**, pagat): only possible when Skíz and XXI are on
  opposite teams. If the Skíz and XXI fall in the **same** trick from two
  partners, it is **not** a catch (no payment). Loser of the XXI wears the
  mayor's hat (cosmetic; out of scope).
- **Double game** (**C §4.5**): *"a parti semmilyen körülmény esetén sem
  számítandó"* — when double game is **announced**, the ordinary game is not
  scored for the announcing side. The announcer commits that their side makes
  **≥ 71**. If they lose it **and** the opponents also make the game, **both**
  count for the opponents. (See the scoring tables in §7 for the exact
  multipliers.)
- **Volát** (**C §4.6**): same "parti not separately counted" structure; commits
  to **all** tricks.
- **C §7.1:** after **volát** is announced you may **not** announce trull, four
  kings, or double game.
- **C §7.2:** you **may** announce double game and volát at the same turn.
- **Silent trull / silent four kings are NOT scored in addition to volát**
  (pagat): a side that wins every trick scores volát but not silent trull/four
  kings. (Announced trull/four kings still score.) This produces the classic
  "give up one trick for 2+1+1 instead of volát's 3" incentive — kept as-is
  (the competition does not adopt the Kégl fix that removes it).

### 5.3 Kontra chain (C §4.8)

`kontra → rekontra → szubkontra → hirskontra`, each **doubling** the value of the
specific item it targets. (Note: the competition chain stops at **hirskontra** —
4 levels; pagat additionally lists *mordkontra*, which we do **not** implement.)

- **You kontra the other side's announcements; you rekontra your own.** Only an
  **opponent** of the announcing side may say *kontra* (×2) or *szubkontra* (×8);
  only the **announcing side** may say *rekontra* (×4) or *hirskontra* (×16). The
  doublings therefore alternate sides:
  `announce → kontra (opp.) → rekontra (own) → szubkontra (opp.) → hirskontra (own)`.
- The **game** is kontra'd the same way: the declarer's side is the "announcing"
  side of the game (it took the contract), so the **defenders** kontra the game,
  the declarer's side rekontras, and so on.
- Each kontra targets **one specific item** (the game, or a named bonus). They
  are independent — e.g. "kontra the pagátultimó", "kontra the four kings and the
  game".
- Each announced bonus, and the game, carries its **own** independent kontra
  level.

### 5.4 Declaring 8 / 9 tarokks (*tarokkszám*) (C §4.1)

- **8 tarokks → 1 point; 9 tarokks → 2 points** (**C §4.1**).
- It is illegal to declare 8 if you actually hold 9 (pagat).
- **Mandatory** when you announce **pagátultimó**, or when you **kontra a
  pagátultimó** (**C §4.1.3**): you must declare 8/9 tarokks if you hold them.
- **C §4.1.1:** declaring the tarokk count is *"licitfenntartó hatályú"* —
  has **bid-sustaining effect**: everyone must pass after an announcement of 8/9
  tarokks before play can start.
- **C §4.1.2:** before the partnerships are known, a tarokk-count declaration is
  *"addressed to the last speaker"* — i.e. it carries the same team-allegiance
  signalling as a bonus announcement (treat it like an announcement for the
  side-deduction rules of §5.5). Violation = RENONSZ. *(This differs from base
  pagat, where declarations carry no allegiance.)*
- **Payment** (**C §4.1**) The opposing team pays the declaring team: a single point
  for 8 tarokks, 2 points for 9 tarokks. Both players may declare their tarokk count,
  in this case they get the point(s) for both announcements.

### 5.5 Side-deduction rules for announcements (pagat §procedure)

Needed so the engine knows which team an announcement/kontra belongs to (and
hence what is a legal kontra):

- If a player makes an announcement and their side is not otherwise provable,
  they are **assumed to be on the same side as the most recent player who
  announced or kontra'd** anything. With nothing yet said, an announcer is
  **assumed to be the declarer's partner**.
- Therefore, to announce something while being **against** the last announcer (or
  against the declarer if nothing has been said), you **must also kontra/rekontra**
  one of the other side's items, to reveal your side.
- If your side is already **known** (from a cue-bid/yielded call, or from a prior
  announcement of yours), you may announce freely without a kontra.
- The competition treats **tarokk-count declarations as side-carrying** (per
  **C §4.1.2**) — unlike base pagat. Apply the same deduction rule to them.

### 5.6 Restrictions on double game / volát announcements (pagat)

- You cannot announce **both** double game and volát **at the same turn**…
  - …but **C §7.2** explicitly allows announcing double and volát together.
    **[UNCLEAR / CONFLICT]** — pagat's base rule and **C §7.2** disagree.
    **Follow the competition: double + volát may be announced together.**
  - Double + volát can only be announced in this order, even within the same turn.
- You cannot announce double game after **your team has already announced volát**
  (pagat) — and **C §7.1** forbids trull / four kings / double game **after volát
  is announced** at all. Follow **C §7.1**.

### 5.7 Trull (*tulétroá*) announcement conventions (C §4.2)

Because partnerships are hidden, announcing trull does more than commit your side
to winning all three honours — it **promises specific honour holdings in the
announcer's own hand**, and which holding it promises depends on *when* (which
round of the announcement phase) and *who* (role in the auction) announces it.
Announcing trull without the promised cards is illegal (RENONSZ under **C §10.5**
→ the engine must not offer it). So the legal-action generator must gate
"announce trull" on the announcer's hand satisfying the relevant promise below.

Terms: **high honours** = Skíz, XXI. *First round* = a player's **first** turn to
speak in the announcement phase; *second round* = any **later** turn. *Bidders* =
players who made a positive bid during the auction.

**Trull announced in the FIRST round** (declarer's side):

| Announcer | Auction context | Must hold | Source |
|---|---|---|---|
| **Declarer** | simple game (no invite / yield), fewer than three bidders | **both** high honours (Skíz **and** XXI) | C §4.2.1 |
| **Declarer** | simple game, **three bidders** | the **Skíz** | C §4.2.1 |
| **Declarer** | invited or yielded game | **at least one** high honour | C §4.2.2 |
| **Partner** (holder of the called card) | any | **at least one** high honour | C §4.2.3 |

**Trull announced in the SECOND (or any later) round** — by anyone, on either
side:

| Must hold | Source |
|---|---|
| **at least one** high honour | C §4.2.4 |

**Inviter / yielder picking up the trull** (**C §4.2.2**, *Megjegyzés*): in an
invited or yielded game, **if the declarer did not** announce trull, the player
who invited or yielded (i.e. the declarer's partner) may announce it instead.
Their trull then promises **two honours**.

Notes:

- The **three-bidder exception** (declarer promises only the Skíz, not both)
  reflects that with three bidders each holds an honour, so the three honours are
  split one-per-bidder and the declarer cannot guarantee both high honours — only
  the Skíz.
- These promises are simultaneously a **legality** constraint (does the announcer
  actually hold the promised cards) and an **information** signal (they let the
  other players locate the high honours — which is exactly why mis-stating or
  omitting them is RENONSZ under **C §10.5**). They do **not** change the bonus's
  value or how it scores (§5.2 / §7.3).
- **Defenders' trull:** a defender's trull follows the second-round rule (≥ 1
  high honour, **C §4.2.4**); to announce it the defender must also reveal their
  side per the side-deduction rules of §5.5.

---

## 6. The play

- Forehand (dealer's right) **leads** the first trick; winner of each trick leads
  the next.
- **Follow-suit rules:** you must follow the suit led if able. If you have no card
  of the led suit you **must play a tarokk** if you have one. Only with neither
  the led suit nor any tarokk may you play anything. A tarokk lead must be
  followed with a tarokk if you have one.
- The trick is won by the **highest tarokk** in it, or, if none, by the **highest
  card of the led suit**.
- **Pagátultimó obligation:** if your side announced pagátultimó, the pagát holder
  **must keep the pagát as long as legally possible** — only playing it when
  forced by the follow-suit / must-trump rules — even if it can no longer win
  (**C §10.6** treats premature play of a held card as RENONSZ; pagat §play).
- **C §8.1:** the **last completed trick** may be reviewed until the next card is
  led. (UI nicety; affects information, not legality.)
- Nine tricks; then score.

---

## 7. Scoring one deal

### 7.1 Winning the game (C §9)

- Count card points in tricks. The **declarer's side adds their own discard**;
  the **opponents add the combined three-player discard**.
- **Declarer's side wins** the game with **≥ 48** card points.
- **Opponents win** with **≥ 47** — **regardless of whether the game was
  kontra'd** (**C §9** explicitly: no "47‑47 → declarer" rule; the pagat variant
  that raises the opponents' target to 48 under kontra is **not** used).
- **Double game**: a side taking **≥ 71** (other side ≤ 23).
- **Volát**: a side taking **all nine tricks**.

### 7.2 Base game value × multiplier

Base game value by bid: three = 1, two = 2, one = 3, solo = 4.

Without any announcement or kontra, **only one** of game / double game / volát
scores (the best the result supports), as a multiplier on the base value:

| Base bid | game | silent double | silent volát | announced double | announced volát |
|---|---|---|---|---|---|
| three (1) | 1 | 2 | 3 | 4 | 6 |
| two (2) | 2 | 4 | 6 | 8 | 12 |
| one (3) | 3 | 6 | 9 | 12 | 18 |
| solo (4) | 4 | 8 | 12 | 16 | 24 |

**Base game-score multipliers for the declarer's team** (positive = declarer's
team wins; negative = opponents win), by card points / tricks taken by the
**declarer's** team (pagat scoring table — implement exactly):

| Announcements | no trick | ≤ 23 | 24–47 | 48–70 | ≥ 71 | all tricks |
|---|---|---|---|---|---|---|
| Nothing | −3 | −2 | −1 | +1 | +2 | +3 |
| Double game | −7 | −6 | −5 | −4 | +4 | +7 |
| Volát | −9 | −8 | −7 | −6 | −6 | +6 |
| Double game + volát | −13 | −12 | −11 | −10 | −2 | +10 |
| Kontra the game | −5 | −4 | −2 | +2 | +4 | +5 |
| Kontra game; opp. announce double | −9 | −6 | +2 | +6 | +8 | +9 |
| Double game; opp. kontra the double | −11 | −10 | −9 | −8 | +8 | +11 |
| Double game; opp. kontra double + game | −13 | −12 | −10 | −6 | +10 | +13 |

These cover the common cases; the general rules behind them (pagat §scoring):

1. With no announced double/volát and no kontra of the game, exactly one of game
   / double game / volát is scored.
2. An **announced double game** cannot also score the ordinary game for that
   side, but **volát can be scored on top of an announced double game** if all
   tricks are won. If the announcing side loses the game too, the opponents
   score the ordinary game **in addition** to the failed announced double
   (5× game total), or their own silent double/volát instead of the ordinary
   game if they made one.
3. **Announced double + announced volát** are scored **separately** (can win one,
   lose the other). The ordinary game is scored only if the announcing side
   loses it (then the opponents get it).
4. **Announced volát (no double)**: volát is scored won/lost; the announcing side
   can score nothing for game/double, but opponents score the game if they win
   it.
5. A **kontra'd game is always scored**. A silent double or volát by either side
   is scored **in addition** to the kontra'd game (but not both double and
   volát).
6. If the game is kontra'd/rekontra'd **and** double/volát is announced (either
   team), the game **and** the announcements both score; a silent volát can be
   added on top of a kontra'd game + announced double.

### 7.3 Other bonuses / declarations (flat, independent of base game value)

| Item | Silent | Announced | Paid by |
|---|---|---|---|
| Trull | 1 | 2 | losing team → winning team |
| Four kings | 1 | 2 | losing team → winning team |
| Pagátultimó | 5 | 10 | losing team → winning team |
| XXI-catch | 21 | 42 | side that lost the XXI → side that caught it |
| 8 tarokks | 1 | 1 | other team → announcing team |
| 9 tarokks | 2 | 2 | other team → announcing team |

- **Per-side scoring (see §5.2).** Each announcement or silent bonus is scored
  for the side that made it. An **announced** bonus that fails pays the
  **announced** amount to the other side. Both sides may have announced the same
  bonus — each pays the other if its own announcement fails — and a side that
  makes a bonus **silently** still scores its silent value even if the other side
  announced (and lost) the same bonus. So one team can score the same bonus
  twice: the opponents' failed announcement **plus** its own silent making of it.
- **Playing alone** (declarer called their own XX, or called a discarded tarokk):
  the declarer settles with **each** of the three opponents separately, so the
  per-deal value to the lone declarer is effectively **×3** (pagat §scoring).

### 7.4 Settlement form

Pagat's model is per-hand cash settlement where each loser pays one winner; on a
score sheet, receipts are + and payments −, summing to zero across players. For
OpenSpiel, expose **per-player returns that sum to zero** for the deal.
All players on each side get the same amount. When the declarer is not alone,
all players get the same amount of points in absolute value. When the declarer is
alone, each other player pays him/gets paid by him, meaning that the declarers score is
(-3)× the score of any other player.

---

## 8. RENONSZ (rule violations) → engine mapping

The competition's §10 (and the inline RENONSZ tags) are **table-play penalties**
for human mistakes. In a game engine these are handled in one of two ways:

1. **RENONSZ-triggering actions are simply illegal moves** — the engine
   never offers them, so no penalty is needed. Examples: discarding an
   honour/king/XX/invited tarokk (C §6.1), making a cue bid/yield without the
   promised card (C §5.2.7), the declarer calling themselves with a non-XX card
   (C §5.1.4), failing to call an accepted invite (C §5.2.1), playing a card that
   breaks follow-suit / playing a held pagát early (C §10.6), bidding without an
   honour outside the 4th-seat trial.

The monetary RENONSZ schedule itself (**C §10.7:** the offender pays achieved +
announced figures, min 5 / max 10, to each of the other three; two offenders pay
double, min 10 / max 20) is **out of scope** for a clean-move engine — there is
no illegal move to penalize. Document only.

---

*Sources: `rules.html` (pagat.com, John McLeod, "Hungarian Tarokk") and
`competition-rules.txt` (Paskievics tournament rules, 2018‑03‑01).*