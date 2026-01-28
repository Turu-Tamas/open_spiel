#ifndef OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SKART_H_
#define OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SKART_H_

#include "spiel.h"
#include "game_phase.h"
#include "setup.h"
#include "bidding.h"
#include "announcements.h"

namespace open_spiel {
namespace hungarian_tarok {
    class SkartPhase : public GamePhase {
    public:
        SkartPhase(Deck deck, Player declarer) : deck_(deck), declarer_(declarer) {}
        ~SkartPhase() override = default;

        Player CurrentPlayer() const override {
            return current_player_;
        }
        std::vector<Action> LegalActions() const override {
            SPIEL_CHECK_FALSE(PhaseOver());
            std::vector<Action> actions;
            for (Card card = 0; card < kDeckSize; ++card) {
                if (deck_[card] == current_player_) {
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

            if (current_player_ == declarer_) {
                deck_[action] = kDeclarerSkart;
            } else {
                deck_[action] = kOpponentsSkart;
            }
            cards_discarded_++;
        }
        bool PhaseOver() const override {
            return cards_discarded_ == kTalonSize;
        }
        bool GameOver() const override {
            return false;
        }
        std::unique_ptr<GamePhase> NextPhase() const override {
            SPIEL_CHECK_TRUE(PhaseOver());
            return std::make_unique<AnnouncementsPhase>(declarer_, deck_);
        }
    private:
        Deck deck_;
        Player declarer_;
        Player current_player_ = 0;
        int cards_discarded_ = 0;
    };
} // namespace hungarian_tarok
} // namespace open_spiel

#endif  // OPEN_SPIEL_GAMES_HUNGARIAN_TAROK_SKART_H_