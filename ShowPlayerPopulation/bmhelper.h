#pragma once

#include <map>
#include <string>
#include "bakkesmod/wrappers/MatchmakingWrapper.h"

namespace bmhelper {
        std::map<PlaylistIds, std::string> playlist_ids_str = {
                {                PlaylistIds::Unknown,                 "Unknown"}, // NO MATCHING PLAYLIST
                {                 PlaylistIds::Casual,                  "Casual"}, // NO MATCHING PLAYLIST
                {                   PlaylistIds::Duel,                    "Duel"},
                {                PlaylistIds::Doubles,                 "Doubles"},
                {               PlaylistIds::Standard,                "Standard"},
                {                  PlaylistIds::Chaos,                   "Chaos"},
                {           PlaylistIds::PrivateMatch,            "PrivateMatch"},
                {                 PlaylistIds::Season,                  "Season"},
                {     PlaylistIds::OfflineSplitscreen,      "OfflineSplitscreen"},
                {               PlaylistIds::Training,                "Training"},
                {         PlaylistIds::RankedSoloDuel,          "RankedSoloDuel"},
                {      PlaylistIds::RankedTeamDoubles,       "RankedTeamDoubles"},
                {         PlaylistIds::RankedStandard,          "RankedStandard"},
                {       PlaylistIds::SnowDayPromotion,        "SnowDayPromotion"},
                {           PlaylistIds::Experimental,            "Experimental"}, // NO MATCHING PLAYLIST
                {      PlaylistIds::BasketballDoubles,       "BasketballDoubles"}, // NO MATCHING PLAYLIST
                {                 PlaylistIds::Rumble,                  "Rumble"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::Workshop,                "Workshop"},
                {      PlaylistIds::UGCTrainingEditor,       "UGCTrainingEditor"},
                {            PlaylistIds::UGCTraining,             "UGCTraining"},
                {             PlaylistIds::Tournament,              "Tournament"},
                {               PlaylistIds::Breakout,                "Breakout"}, // NO MATCHING PLAYLIST
                {       PlaylistIds::TenthAnniversary,        "TenthAnniversary"}, // NO MATCHING PLAYLIST
                {                 PlaylistIds::FaceIt,                  "FaceIt"},
                {PlaylistIds::RankedBasketballDoubles, "RankedBasketballDoubles"},
                {           PlaylistIds::RankedRumble,            "RankedRumble"},
                {         PlaylistIds::RankedBreakout,          "RankedBreakout"},
                {          PlaylistIds::RankedSnowDay,           "RankedSnowDay"},
                {            PlaylistIds::HauntedBall,             "HauntedBall"}, // NO MATCHING PLAYLIST
                {              PlaylistIds::BeachBall,               "BeachBall"}, // NO MATCHING PLAYLIST
                {                  PlaylistIds::Rugby,                   "Rugby"},
                {         PlaylistIds::AutoTournament,          "AutoTournament"},
                {             PlaylistIds::RocketLabs,              "RocketLabs"}, // NO MATCHING PLAYLIST
                {                PlaylistIds::RumShot,                 "RumShot"}, // NO MATCHING PLAYLIST
                {                PlaylistIds::GodBall,                 "GodBall"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::CoopVsAI,                "CoopVsAI"}, // NO MATCHING PLAYLIST
                {             PlaylistIds::BoomerBall,              "BoomerBall"},
                {         PlaylistIds::GodBallDoubles,          "GodBallDoubles"}, // NO MATCHING PLAYLIST
                {         PlaylistIds::SpecialSnowDay,          "SpecialSnowDay"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::Football,                "Football"},
                {                  PlaylistIds::Cubic,                   "Cubic"}, // NO MATCHING PLAYLIST
                {         PlaylistIds::TacticalRumble,          "TacticalRumble"}, // NO MATCHING PLAYLIST
                {           PlaylistIds::SpringLoaded,            "SpringLoaded"}, // NO MATCHING PLAYLIST
                {             PlaylistIds::SpeedDemon,              "SpeedDemon"},
                {               PlaylistIds::RumbleBM,                "RumbleBM"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::Knockout,                "Knockout"},
                {             PlaylistIds::Thirdwheel,              "Thirdwheel"}, // NO MATCHING PLAYLIST
                {          PlaylistIds::MagnusFutball,           "MagnusFutball"}, // NO MATCHING PLAYLIST
        };

        std::map<PlaylistIds, std::string> playlist_ids_str_spaced = {
                {                PlaylistIds::Unknown,                   "Unknown"}, // NO MATCHING PLAYLIST
                {                 PlaylistIds::Casual,                    "Casual"}, // NO MATCHING PLAYLIST
                {                   PlaylistIds::Duel,               "Casual Duel"},
                {                PlaylistIds::Doubles,            "Casual Doubles"},
                {               PlaylistIds::Standard,           "Casual Standard"},
                {                  PlaylistIds::Chaos,              "Casual Chaos"},
                {           PlaylistIds::PrivateMatch,             "Private Match"},
                {                 PlaylistIds::Season,                    "Season"},
                {     PlaylistIds::OfflineSplitscreen,       "Offline Splitscreen"},
                {               PlaylistIds::Training,                  "Training"},
                {         PlaylistIds::RankedSoloDuel,          "Ranked Solo Duel"},
                {      PlaylistIds::RankedTeamDoubles,       "Ranked Team Doubles"},
                {         PlaylistIds::RankedStandard,           "Ranked Standard"},
                {       PlaylistIds::SnowDayPromotion,           "Casual Snow Day"},
                {           PlaylistIds::Experimental,              "Experimental"}, // NO MATCHING PLAYLIST
                {      PlaylistIds::BasketballDoubles,        "Basketball Doubles"}, // NO MATCHING PLAYLIST
                {                 PlaylistIds::Rumble,             "Casual Rumble"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::Workshop,                  "Workshop"},
                {      PlaylistIds::UGCTrainingEditor,       "UGC Training Editor"},
                {            PlaylistIds::UGCTraining,              "UGC Training"},
                {             PlaylistIds::Tournament,                "Tournament"},
                {               PlaylistIds::Breakout,                  "Breakout"}, // NO MATCHING PLAYLIST
                {       PlaylistIds::TenthAnniversary,         "Tenth Anniversary"}, // NO MATCHING PLAYLIST
                {                 PlaylistIds::FaceIt,                   "Face It"},
                {PlaylistIds::RankedBasketballDoubles, "Ranked Basketball Doubles"},
                {           PlaylistIds::RankedRumble,             "Ranked Rumble"},
                {         PlaylistIds::RankedBreakout,           "Ranked Breakout"},
                {          PlaylistIds::RankedSnowDay,           "Ranked Snow Day"},
                {            PlaylistIds::HauntedBall,              "Haunted Ball"}, // NO MATCHING PLAYLIST
                {              PlaylistIds::BeachBall,                "Beach Ball"}, // NO MATCHING PLAYLIST
                {                  PlaylistIds::Rugby,                     "Rugby"},
                {         PlaylistIds::AutoTournament,           "Auto Tournament"},
                {             PlaylistIds::RocketLabs,               "Rocket Labs"}, // NO MATCHING PLAYLIST
                {                PlaylistIds::RumShot,                  "Rum Shot"}, // NO MATCHING PLAYLIST
                {                PlaylistIds::GodBall,                  "God Ball"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::CoopVsAI,                "Coop Vs AI"}, // NO MATCHING PLAYLIST
                {             PlaylistIds::BoomerBall,               "Boomer Ball"},
                {         PlaylistIds::GodBallDoubles,          "God Ball Doubles"}, // NO MATCHING PLAYLIST
                {         PlaylistIds::SpecialSnowDay,           "Special SnowDay"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::Football,                  "Football"},
                {                  PlaylistIds::Cubic,                     "Cubic"}, // NO MATCHING PLAYLIST
                {         PlaylistIds::TacticalRumble,           "Tactical Rumble"}, // NO MATCHING PLAYLIST
                {           PlaylistIds::SpringLoaded,             "Spring Loaded"}, // NO MATCHING PLAYLIST
                {             PlaylistIds::SpeedDemon,               "Speed Demon"},
                {               PlaylistIds::RumbleBM,                 "Rumble BM"}, // NO MATCHING PLAYLIST
                {               PlaylistIds::Knockout,                  "Knockout"},
                {             PlaylistIds::Thirdwheel,                "Thirdwheel"}, // NO MATCHING PLAYLIST
                {          PlaylistIds::MagnusFutball,            "Magnus Futball"}, // NO MATCHING PLAYLIST
        };
}  // namespace bmhelper