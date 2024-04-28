#pragma once
#include <chrono>

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
#include "bakkesmod/plugin/pluginwindow.h"

#include "bmhelper.h"
#include "imgui.h"
#include "implot.h"

/*
 * SAMPLE:
 *
 * 13219:[19:48:07] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:48:07.8930216+0000, TIMES UPDATED: 0,  POPULATION: 326275
 * 13272:[19:49:46] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:49:46.8816692+0000, TIMES UPDATED: 1,  POPULATION: 325712
 * 13426:[19:54:48] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:54:48.8792777+0000, TIMES UPDATED: 2,  POPULATION: 324136
 * 13578:[19:59:50] [bakkesmod] [class ShowPlayerPopulation] POPULATION UPDATED AT TIME:
 2024-04-08T00:59:50.8810702+0000, TIMES UPDATED: 3,  POPULATION: 322139
 */

class ShowPlayerPopulation :
        public BakkesMod::Plugin::BakkesModPlugin,
        public BakkesMod::Plugin::PluginSettingsWindow,
        public BakkesMod::Plugin::PluginWindow {
private:
        // variables pertaining to the plugin's functionality

        static const int DONT_SHOW_POP_BELOW_THRESHOLD = 10;
        // Why less than 10?
        // Developers tend to jump into modes at their whim
        // The population numbers are still tracked no matter who is in the
        // mode, so you would see 1 or 2 people in a mode that isn't available
        // to the public. I'm guessing, if they don't want to show population
        // data to begin with, maybe they don't want people stalking those
        // people? like, "why are they blah blah when blah blah?" ... idk. It
        // just doesn't seem very relevant to track the 1 or 2 or 9 people
        // playing in an inaccessible mode. .... except FaceIt for some reason.
        // and 10 ... because... to give an opportunity to
        // catch enough people in the custom training editor

        static const std::string          CMD_PREFIX;
        static const std::chrono::seconds GRAPH_DATA_MASSAGE_TIMEOUT;
        const std::filesystem::path       RECORD_POPULATION_FILE =
                gameWrapper->GetDataFolder().append("ShowPlayerPopulation\\RecordPopulationData.csv");
        const std::filesystem::path POP_NUMBER_PLACEMENTS_FILE =
                gameWrapper->GetDataFolder().append("ShowPlayerPopulation\\FirstTimePopulationNumberPlacements.txt");
        const std::string DATETIME_FORMAT_STR = "{0:%F}T{0:%T%z}";
        const std::string DATETIME_PARSE_STR  = "%FT%T%z";

        // flags for different points in the plugin
        bool is_overlay_open       = false;
        bool lock_overlay          = false;
        bool in_main_menu          = false;
        bool in_playlist_menu      = false;
        bool in_game_menu          = false;
        bool show_in_main_menu     = false;
        bool show_in_playlist_menu = false;
        bool show_in_game_menu     = false;
        bool DO_CHECK              = false;

        ImFont * overlay_font_18           = nullptr;
        ImFont * overlay_font_22           = nullptr;
        ImVec4   chosen_overlay_color      = {1.0f, 1.0f, 1.0f, 0.9f};
        ImVec4   chosen_overlay_text_color = {0.0f, 0.0f, 0.0f, 1.0f};
        ImColor  col_black                 = ImColor {
                ImVec4 {0.0f, 0.0f, 0.0f, 1.0f}
        };
        ImColor col_white = ImColor {
                ImVec4 {1.0f, 1.0f, 1.0f, 1.0f}
        };

        // miscellaneous helper data. graphing should go here.
        struct thrair {  // three pair
                std::vector<std::chrono::zoned_seconds> t;
                std::vector<float>                      xs;
                std::vector<float>                      ys;
        };
        const std::vector<std::string> SHOWN_PLAYLIST_POPS =
                {"Casual", "Competitive", "Tournament", "Training", "Offline", "Private Match"};
        std::map<std::string, std::vector<std::pair<PlaylistId, int>>> population_data;
        int                                                            TOTAL_IN_GAME_POP = 0;
        std::chrono::zoned_seconds   last_massage_update {std::chrono::current_zone()};
        bool                         has_graph_data      = false;
        bool                         data_header_is_open = false;
        bool                         graph_total_pop     = true;
        thrair                       graph_total_pop_data;  // {times, xs, ys}
        std::map<PlaylistId, thrair> graph_data;            // [PlaylistId] -> {times, xs, ys}
        std::map<PlaylistId, bool>   graph_flags = []() {
                std::map<PlaylistId, bool> tmp;
                for (const auto & item : bmhelper::playlist_ids_str) {
                        tmp[item.first] = false;
                };
                return tmp;
        }();
        std::map<PlaylistId, bool> graph_flags_selected {graph_flags};

        void print_graph_data();

        // members pertaining to data functionality
        struct token {
                std::chrono::zoned_seconds zt;
                int                        total_pop;
                int                        total_players_online;  // I've never seen it be unequal to total_pop
                std::map<PlaylistId, int>  playlist_pop;
        };

        // a bank full of tokens
        // it holds all the data during the operation of the plugin
        // ... instead of reading in and out of a file...
        // would be neat to separate out an interface
        // and that would take desining another class to basically
        // -> record, -> write -> read -> save... one class implementation would read / write to the file for every
        // operation; another implementation would just use the queue...
        // but I'm not doing that right now

        // it's a deque, as to support iteration through its elements
        std::deque<token> bank;

        bool      keep_all_data = false;
        const int hours_min     = 0;
        const int hours_max     = 168;
        int       hours_kept    = 24;

        // flags for showing numbers above playlists
        // ordered 1-6, top left to bottom right
        bool slot1 = false;
        bool slot2 = false;
        bool slot3 = false;
        bool slot4 = false;
        bool slot5 = false;
        bool slot6 = false;
        bool show_all;

        ImVec2 onepos, twopos, threepos, fourpos, fivepos, sixpos;
        ImVec2 slot1_init_pos, slot2_init_pos, slot3_init_pos, slot4_init_pos, slot5_init_pos, slot6_init_pos;
        void   SNAPSHOT_PLAYLIST_POSITIONS();
        void   print_bank_info();
        void   GET_DEFAULT_POP_NUMBER_PLACEMENTS();

        // these may end up going away
        bool showstats;
        bool curiouser;

        // member functions pertaining to general functionality
        void init_datafile();
        void init_cvars();
        void init_hooked_events();
        void init_graph_data();
        void CHECK_NOW();
        void record_population();
        void prepare_data();

        void add_last_entry_to_graph_data();
        void massage_graph_data();
        void prune_data();
        void write_data_to_file();

        // helper functions
        ShowPlayerPopulation::token get_first_bank_entry();
        ShowPlayerPopulation::token get_last_bank_entry();
        std::string                 get_current_datetime_str();
        std::chrono::zoned_seconds  get_timepoint_from_str(std::string);
        void                        SET_WHICH_MENU_I_AM_IN();

        void add_notifier(
                std::string                                   cmd_name,
                std::function<void(std::vector<std::string>)> do_func,
                std::string                                   desc,
                unsigned char                                 PERMISSIONS) const;

public:
        void onLoad() override;
        void onUnload() override;

        void        RenderSettings() override;
        std::string GetPluginName() override;
        void        SetImGuiContext(uintptr_t ctx) override;

        // inherit from
        //				public BakkesMod::Plugin::PluginWindow
        //	 for the following
        void        OnOpen() override;
        void        OnClose() override;
        void        Render() override;
        std::string GetMenuName() override;
        std::string GetMenuTitle() override;
        bool        IsActiveOverlay() override;
        bool        ShouldBlockInput() override;
};
