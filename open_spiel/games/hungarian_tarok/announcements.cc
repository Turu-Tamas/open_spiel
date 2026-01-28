#include "announcements.h"
#include "card.h"

namespace open_spiel {
namespace hungarian_tarok {
    std::vector<Action> AnnouncementsPhase::LegalActions() const {
        SPIEL_CHECK_FALSE(PhaseOver());
        if (!partner_called_) {
            if (deck_[MakeTarok(20)] == declarer_) {
                // declarer can call self with XX
                return {kActionCallPartner, kActionCallSelf};
            }
            return {kActionCallPartner};
        }

        std::vector<Action> actions;
        const Side &current_side = CurrentSide();
        const Side &other_side = OtherSide();
        // separate loops so actions are sorted
        for (int i = 0; i < kNumAnnouncementTypes; ++i) {
            AnnouncementType type = static_cast<AnnouncementType>(i);
            if (!current_side.announced[i]) { // announce only if not announced yet
                actions.push_back(AnnouncementAction::AnnounceAction(type));
            }
        }
        for (int i = 0; i < kNumAnnouncementTypes; ++i) {
            AnnouncementType type = static_cast<AnnouncementType>(i);
            if (other_side.announced[i] && other_side.contra_level[i] % 2 == 0) { // contra only if other side announced and not contra'd yet (or re-contra'd)
                actions.push_back(AnnouncementAction::ContraAction(type));
            }
        }
        for (int i = 0; i < kNumAnnouncementTypes; ++i) {
            AnnouncementType type = static_cast<AnnouncementType>(i);
            if (current_side.contra_level[i] % 2 == 1) { // re-contra only if contra'd already (or re-contra subcontra'd)
                actions.push_back(AnnouncementAction::ReContraAction(type));
            }
        }
        actions.push_back(AnnouncementAction::PassAction());
        return actions;
    }

    void AnnouncementsPhase::DoApplyAction(Action action) {
        SPIEL_CHECK_FALSE(PhaseOver());
        std::vector<Action> legal_actions = LegalActions();
        SPIEL_CHECK_TRUE(std::find(legal_actions.begin(), legal_actions.end(), action) != legal_actions.end());
        if (!partner_called_) {
            if (action == kActionCallPartner) {
                // call highest tarok not in declarer's hand
                for (int rank = 20; rank >= 1; --rank) {
                    Card card = MakeTarok(rank);
                    if (deck_[card] != declarer_) {
                        partner_ = deck_[card];
                        break;
                    }
                }
                SPIEL_CHECK_TRUE(partner_.has_value());
            } // else partner = std::nullopt
            partner_called_ = true;
            // next player is the player after declarer
            current_player_ = (declarer_ + 1) % kNumPlayers;
            last_to_speak_ = declarer_;
            return;
        }

        if (action == AnnouncementAction::PassAction()) {
            current_player_ = (current_player_ + 1) % kNumPlayers;
            if (current_player_ == last_to_speak_) {
                current_player_ = kTerminalPlayerId; // end of phase
            }
            return;
        }

        AnnouncementAction ann_action = AnnouncementAction::FromAction(action);
        Side &current_side = CurrentSide();
        Side &other_side = OtherSide();
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
        SPIEL_CHECK_TRUE(std::find(legal_actions.begin(), legal_actions.end(), action) != legal_actions.end());
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