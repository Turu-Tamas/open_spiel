#include "spiel.h"
#include "game_phase.h"
#include "setup.h"
#include "bidding.h"

#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PLAY_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PLAY_H_

namespace open_spiel {
namespace hungarian_tarok {
    const int kNumRounds = 9;

    class PlayPhase : public GamePhase {
    public:
        PlayPhase(SetupPhase &setup_phase, Player declarer) : deck_(setup_phase.GetDeck()), declarer_(declarer) {}
        ~PlayPhase() override = default;

        Player CurrentPlayer() const override {
            return current_player_;
        }
        std::vector<Action> LegalActions() const override {
            SPIEL_CHECK_FALSE(PhaseOver());

            std::vector<Action> actions;
            auto can_play = [&](Card card) {
                return trick_cards_.empty() || CardSuit(trick_cards_.front()) == CardSuit(card);
            };
            for (Card card = 0; card < kDeckSize; ++card) {
                if (deck_[card] == current_player_ && can_play(card)) {
                    actions.push_back(card);
                }
            }
            if (!actions.empty())
                return actions;

            // no cards of the leading suit
            bool has_tarok = std::any_of(deck_.begin(), deck_.end(), [&](Player p, Card c) {
                return p == current_player_ && CardSuit(c) == Suit::kTarok;
            });
            for (Card card = 0; card < kDeckSize; ++card) {
                if (deck_[card] != current_player_)
                    continue;
                // must play tarok if has one
                if (!has_tarok || CardSuit(card) == Suit::kTarok) {
                    actions.push_back(card);
                }
            }
            return actions;
        }
        void DoApplyAction(Action action) override {
            SPIEL_CHECK_GE(action, 0);
            SPIEL_CHECK_LT(action, kDeckSize);
            SPIEL_CHECK_EQ(deck_[action], current_player_);
            SPIEL_CHECK_FALSE(PhaseOver());

            // play the card
            deck_[action] = kCurrentTrick; // mark as played in round
            trick_cards_.push_back(action);
            if (trick_cards_.size() == kNumPlayers) {
                // trick is complete
                ResolveTrick();
            } else {
                // advance to next player in trick
                current_player_ = (current_player_ + 1) % kNumPlayers;
            }
        }
        bool PhaseOver() const override {
            return round_ >= kNumRounds;
        }
        bool GameOver() const override {
            return PhaseOver();
        }

    private:
        Deck &deck_;
        Player declarer_;
        Player current_player_ = 0;
        Player trick_caller_ = 0;
        std::vector<Card> trick_cards_;
        int round_ = 0;

        void ResolveTrick() {
            SPIEL_CHECK_EQ(trick_cards_.size(), kNumPlayers);

            Player trick_winner = trick_caller_;
            Card winning_card = trick_cards_[0];
            for (int i = 1; i < kNumPlayers; ++i) {
                Card card = trick_cards_[i];
                if (CardBeats(card, winning_card)) {
                    winning_card = card;
                    trick_winner = (trick_caller_ + i) % kNumPlayers;
                }
            }

            trick_caller_ = trick_winner;
            current_player_ = trick_winner;
            for (Card card : trick_cards_) {
                deck_[card] = WonCards(trick_winner);
            }
            trick_cards_.clear();

            round_++;
            if (round_ == kNumRounds) {
                current_player_ = kTerminalPlayerId;
            }
        }
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PLAY_H_