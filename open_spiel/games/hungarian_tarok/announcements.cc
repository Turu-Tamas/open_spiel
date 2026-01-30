#include "announcements.h"
#include "card.h"

namespace open_spiel {
namespace hungarian_tarok {
    bool AnnouncementsPhase::CanAnnounceTuletroa() const {
        if (CurrentSide().announced[static_cast<int>(AnnouncementType::kTuletroa)]) {
            return false; // already announced
        }
        if (game_data().full_bid_ && current_player_ == game_data().declarer_ && first_round_) {
            // in full bid in the first round tuletroa from declarer means skiz in hand
            return game_data().deck_[kSkiz] == game_data().declarer_;
        }
        if (current_player_ == game_data().declarer_ && first_round_) {
            // in the first round, tuletroa from declarer means XXI and Skiz in hand
            return game_data().deck_[kXXI] == game_data().declarer_ &&
                   game_data().deck_[kSkiz] == game_data().declarer_;
        }
        if (current_player_ == game_data().partner_ && first_round_) {
            // in the first round, tuletroa from partner means XXI or Skiz in hand
            return game_data().deck_[kXXI] == game_data().partner_ ||
                   game_data().deck_[kSkiz] == game_data().partner_;
        }
        return true;
    }
    std::vector<Action> AnnouncementsPhase::LegalActions() const {
        SPIEL_CHECK_FALSE(PhaseOver());
        if (!partner_called_) {
            if (game_data().deck_[MakeTarok(20)] == game_data().declarer_) {
                // declarer can call self with XX
                return {kActionCallPartner, kActionCallSelf};
            }
            return {kActionCallPartner};
        }

        std::vector<Action> actions;
        const GameData::AnnouncementSide &current_side = CurrentSide();
        const GameData::AnnouncementSide &other_side = OtherSide();
        // separate loops so actions are sorted
        for (int i = 0; i < kNumAnnouncementTypes; ++i) {
            AnnouncementType type = static_cast<AnnouncementType>(i);
            if (current_side.announced[i])
                continue;

            switch (type) {
                case AnnouncementType::kTuletroa:
                    if (CanAnnounceTuletroa())
                        actions.push_back(AnnouncementAction::AnnounceAction(type));
                    break;
                case AnnouncementType::kEightTaroks:
                    if (tarok_counts_[current_player_] == 8)
                        actions.push_back(AnnouncementAction::AnnounceAction(type));
                    break;
                case AnnouncementType::kNineTaroks:
                    if (tarok_counts_[current_player_] == 9)
                        actions.push_back(AnnouncementAction::AnnounceAction(type));
                    break;
                default:
                    actions.push_back(AnnouncementAction::AnnounceAction(type));
            }
        }

        for (int i = 0; i < kNumAnnouncementTypes; ++i) {
            AnnouncementType type = static_cast<AnnouncementType>(i);
            // contra only if other side announced and not contra'd yet (or re-contra'd)
            if (other_side.announced[i]
                && other_side.contra_level[i] % 2 == 0
                && other_side.contra_level[i] <= kMaxContraLevel) {
                actions.push_back(AnnouncementAction::ContraAction(type));
            }
        }

        for (int i = 0; i < kNumAnnouncementTypes; ++i) {
            AnnouncementType type = static_cast<AnnouncementType>(i);
            // re-contra only if contra'd already (or re-contra subcontra'd)
            if (current_side.contra_level[i] % 2 == 1 && current_side.contra_level[i] <= kMaxContraLevel) {
                actions.push_back(AnnouncementAction::ReContraAction(type));
            }
        }
        actions.push_back(AnnouncementAction::PassAction());
        return actions;
    }

    void AnnouncementsPhase::DoApplyAction(Action action) {
        SPIEL_CHECK_FALSE(PhaseOver());
        std::vector<Action> legal_actions = LegalActions();
        SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());

        if (!partner_called_) {
            if (action == kActionCallPartner) {
                // call highest tarok not in declarer's hand
                for (int rank = 20; rank >= 1; --rank) {
                    Card card = MakeTarok(rank);
                    if (game_data().deck_[card] != game_data().declarer_) {
                        game_data().partner_ = game_data().deck_[card];
                        break;
                    }
                }
                SPIEL_CHECK_TRUE(game_data().partner_.has_value());
            } // else partner = std::nullopt
            partner_called_ = true;
            // next player is the player after declarer
            current_player_ = (game_data().declarer_ + 1) % kNumPlayers;
            last_to_speak_ = game_data().declarer_;
            return;
        }

        if (action == AnnouncementAction::PassAction()) {
            current_player_ = (current_player_ + 1) % kNumPlayers;
            if (current_player_ == last_to_speak_) {
                current_player_ = kTerminalPlayerId; // end of phase
            }
            if (current_player_ == game_data().declarer_) {
                first_round_ = false;
            }
            return;
        }

        AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
        GameData::AnnouncementSide &current_side = CurrentSide();
        GameData::AnnouncementSide &other_side = OtherSide();
        int type_index = static_cast<int>(ann_action.type);
        switch (ann_action.level) {
            case AnnouncementAction::Level::kAnnounce:
                current_side.announced[type_index] = true;
                break;
            case AnnouncementAction::Level::kContra:
                other_side.contra_level[type_index]++;
                break;
            case AnnouncementAction::Level::kReContra:
                current_side.contra_level[type_index]++;
                break;
        }
        last_to_speak_ = current_player_;
    }
    std::string AnnouncementsPhase::ActionToString(Player player, Action action) const {
        SPIEL_CHECK_FALSE(PhaseOver());
        std::vector<Action> legal_actions = LegalActions();
        SPIEL_CHECK_TRUE(absl::c_find(legal_actions, action) != legal_actions.end());
        if (!partner_called_) {
            if (action == kActionCallPartner) {
                return "Call partner";
            } else {
                return "Call self (XX)";
            }
        }

        if (action == AnnouncementAction::PassAction()) {
            return "Pass";
        }

        AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
        std::string level_str;
        switch (ann_action.level) {
            case AnnouncementAction::Level::kAnnounce:
                level_str = "Announce ";
                break;
            case AnnouncementAction::Level::kContra:
                level_str = "Contra ";
                break;
            case AnnouncementAction::Level::kReContra:
                level_str = "Re-Contra ";
                break;
        }
        std::string type_str;
        switch (ann_action.type) {
            case AnnouncementType::kFourKings:
                type_str = "Four Kings";
                break;
            case AnnouncementType::kTuletroa:
                type_str = "Tuletroa";
                break;
            case AnnouncementType::kDoubleGame:
                type_str = "Double Game";
                break;
            case AnnouncementType::kVolat:
                type_str = "Volat";
                break;
            case AnnouncementType::kPagatUltimo:
                type_str = "Pagat Ultimo";
                break;
            case AnnouncementType::kXXICapture:
                type_str = "XXI Capture";
                break;
            case AnnouncementType::kEightTaroks:
                type_str = "Eight Taroks";
                break;
            case AnnouncementType::kNineTaroks:
                type_str = "Nine Taroks";
                break;
        }
        return absl::StrCat(level_str, type_str);
    }
} // namespace hungarian_tarok
} // namespace open_spiel