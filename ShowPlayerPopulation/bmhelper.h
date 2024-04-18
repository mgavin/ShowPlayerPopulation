#pragma once

// #include "bakkesmod/wrappers/matchmakingwrapper.h"
// originally in the bakkesmodsdk under wrappers/matchmakingwrapper
// this hasn't been updated in a while.
// this would be the updated list.
enum class PlaylistId {
        Unknown                 = -1337,
        Casual                  = 0,
        Duel                    = 1,
        Doubles                 = 2,
        Standard                = 3,
        Chaos                   = 4,
        PrivateMatch            = 6,
        Season                  = 7,
        OfflineSplitscreen      = 8,
        Training                = 9,
        RankedSoloDuel          = 10,
        RankedTeamDoubles       = 11,
        RankedStandard          = 13,
        SnowDayPromotion        = 15,
        Experimental            = 16,
        BasketballDoubles       = 17,
        Rumble                  = 18,
        Workshop                = 19,
        UGCTrainingEditor       = 20,
        UGCTraining             = 21,
        Tournament              = 22,
        Breakout                = 23,
        LANMatch                = 24,
        TenthAnniversary        = 25,  // not in codered log?
        FaceIt                  = 26,
        RankedBasketballDoubles = 27,
        RankedRumble            = 28,
        RankedBreakout          = 29,
        RankedSnowDay           = 30,
        HauntedBall             = 31,
        BeachBall               = 32,
        Rugby                   = 33,
        AutoTournament          = 34,
        RocketLabs              = 35,
        RumShot                 = 37,
        GodBall                 = 38,
        CoopVsAI                = 40,
        BoomerBall              = 41,
        GodBallDoubles          = 43,
        SpecialSnowDay          = 44,
        Football                = 46,
        Cubic                   = 47,
        TacticalRumble          = 48,
        SpringLoaded            = 49,
        SpeedDemon              = 50,
        RumbleBM                = 52,
        Knockout                = 54,
        Thirdwheel              = 55,
        MagnusFutball           = 62,
        GodBallSpooky           = 64,
        GodBallRicochet         = 66,
        CubicSpooky             = 67,
        GForceFrenzy            = 68,
};

// forward declaration?
namespace bmhelper {}
