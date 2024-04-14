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
}  // namespace bmhelper