#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PLAY_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_PLAY_H_

#include <algorithm>

#include "spiel.h"
#include "game_phase.h"
#include "card.h"

namespace open_spiel {
namespace hungarian_tarok {
    const int kNumRounds = 9;

    class PlayPhase : public GamePhase {
    public:
        explicit PlayPhase(std::unique_ptr<GameData> game_data)
            : GamePhase(std::move(game_data)) {}
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
                if (game_data().deck_[card] == current_player_ && can_play(card)) {
                    actions.push_back(card);
                }
            }
            if (!actions.empty())
                return actions;

            // no cards of the leading suit
            bool has_tarok = false;
            for (Card card = 0; card < kDeckSize; ++card) {
                if (game_data().deck_[card] == current_player_ && CardSuit(card) == Suit::kTarok) {
                    has_tarok = true;
                    break;
                }
            }
            for (Card card = 0; card < kDeckSize; ++card) {
                if (game_data().deck_[card] != current_player_)
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
            SPIEL_CHECK_EQ(game_data().deck_[action], current_player_);
            SPIEL_CHECK_FALSE(PhaseOver());

            // play the card
            game_data().deck_[action] = kCurrentTrick; // mark as played in round
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
        std::unique_ptr<GamePhase> NextPhase() override {
            SPIEL_CHECK_TRUE(PhaseOver());
            return nullptr; // no next phase, game over
        }
        std::unique_ptr<GamePhase> Clone() const override {
            return std::make_unique<PlayPhase>(*this);
        }
        std::string ActionToString(Player player, Action action) const override {
            SPIEL_CHECK_GE(action, 0);
            SPIEL_CHECK_LT(action, kDeckSize);
            return absl::StrCat("Play card ", CardToString(static_cast<Card>(action)));
        }
        std::string ToString() const override {
            return absl::StrCat("Play Phase, round ", round_ + 1, " ", trick_cards_.size(), "/4 cards played" ,
                                "\n", DeckToString(game_data().deck_)); 
        }
    private:
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
                game_data().deck_[card] = WonCards(trick_winner);
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