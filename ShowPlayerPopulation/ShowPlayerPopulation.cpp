#include "ShowPlayerPopulation.h"
#include <format>
#include <fstream>
#include <ranges>

#include "bakkesmod/imgui/imgui_internal.h"

#include "csv.hpp"
#include "implot.h"

#include "HookedEvents.h"
#include "Logger.h"

const std::string ShowPlayerPopulation::cmd_prefix = "rppd_";

BAKKESMOD_PLUGIN(ShowPlayerPopulation, "ShowPlayerPopulation", "0.12.8", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/// <summary>
/// do the following when your plugin is loaded
/// </summary>
void ShowPlayerPopulation::onLoad() {
        // initialize things
        _globalCvarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        init_datafile();

        // cvarManager->registerCvar(
        //         cmd_prefix + "enable_topscroll",
        //         "0",
        //         "Flag to determine showing the top ticker scroll showing player
        //         population");

        CVarWrapper show_overlay_cvar = cvarManager->registerCvar(
                cmd_prefix + "show_overlay",
                "0",
                "Flag to determine if the overlay should be shown",
                false);

        show_overlay_cvar.addOnValueChanged([this](std::string old_value, CVarWrapper new_value) {
                if (is_overlay_open != new_value.getBoolValue()) {
                        gameWrapper->Execute([this](GameWrapper * gw) {
                                // look, sometimes cvarManager just gets fucking
                                // lost.
                                cvarManager->executeCommand("togglemenu ShowPlayerPopulation", false);
                        });
                }
        });

        CVarWrapper hrs_cvar = cvarManager->registerCvar(
                cmd_prefix + "hours",
                "24",
                "Number of hours to keep of population data.",
                false);
        hrs_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { hours_kept = new_value.getIntValue(); });

        HookedEvents::AddHookedEvent(
                "Function ProjectX.OnlineGamePopulation_X.GetPlaylistPopulations",
                // conveniently stops after disconnected
                [this](std::string ev) {
                        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
                        if (mw) {
                                int tmpp = mw.GetTotalPlayersOnline();
                                if (TOTAL_POP != tmpp) {
                                        TOTAL_POP = tmpp;
                                        write_population();
                                }
                        }
                });

        HookedEvents::AddHookedEvent(
                "Function ProjectX.PsyNetConnection_X.EventDisconnected",
                [](std::string eventName) {
                        // conveniently stops the playlist population query
                });
        HookedEvents::AddHookedEvent(
                "Function ProjectX.PsyNetConnection_X.EventConnected",
                [this](std::string eventName) { CHECK_NOW(); });

        HookedEvents::AddHookedEvent(
                "Function TAGame.StatusObserver_MenuStack_TA.HandleMenuChange",
                [this](std::string eventName) { SET_WHICH_MENU_I_AM_IN(); });
        SET_WHICH_MENU_I_AM_IN();
}

/// <summary>
///  do the following when your plugin is unloaded
/// </summary>
void ShowPlayerPopulation::onUnload() {
        // destroy things
        // dont throw here
}

void ShowPlayerPopulation::SNAPSHOT_PLAYLIST_POSITIONS() {
        // THIS SHOULD BE DONE AT MAX SCALE!
        std::ofstream file {POP_NUMBER_PLACEMENTS_FILE, std::ios::app};
        Vector2       disp = gameWrapper->GetScreenSize();
        if (disp.X == -1 && disp.Y == -1) {
                ImVec2 dsps = ImGui::GetIO().DisplaySize;
                disp.X      = dsps.x;
                disp.Y      = dsps.y;
        }
        file << std::vformat(
                "[{}x{}] {} {} {} {} {} {} {} {} {} {} {} {}",
                std::make_format_args(
                        disp.X,
                        disp.Y,
                        static_cast<int>(onepos.x),
                        static_cast<int>(onepos.y),
                        static_cast<int>(twopos.x),
                        static_cast<int>(twopos.y),
                        static_cast<int>(threepos.x),
                        static_cast<int>(threepos.y),
                        static_cast<int>(fourpos.x),
                        static_cast<int>(fourpos.y),
                        static_cast<int>(fivepos.x),
                        static_cast<int>(fivepos.y),
                        static_cast<int>(sixpos.x),
                        static_cast<int>(sixpos.y)))
             << std::endl;
}
/// <summary>
/// This call usually includes ImGui code that is shown and rendered (repeatedly,
/// on every frame rendered) when your plugin is selected in the plugin
/// manager. AFAIK, if your plugin doesn't have an associated *.set file for its
/// settings, this will be used instead.
/// </summary>
void ShowPlayerPopulation::RenderSettings() {
        // ... needs a horizontal variant

        // for imgui plugin window
        // CVarWrapper ticker       = cvarManager->getCvar("rppd_enable_topscroll");
        // bool        top_scroller = ticker.getBoolValue();
        // if (ImGui::Checkbox("Enable ticker scrolling at the top?", &top_scroller)) {
        //        ticker.setValue(top_scroller);
        //}

        CVarWrapper shoverlay  = cvarManager->getCvar(cmd_prefix + "show_overlay");
        bool        bshoverlay = shoverlay.getBoolValue();
        if (ImGui::Checkbox("Show the overlay?", &bshoverlay)) {
                shoverlay.setValue(bshoverlay);
        }

        // show in main menu, show in game, show in playlist menu | flags
        // still need color / transparency /location locking options for the overlays

        if (ImGui::Button("CHECK NOW")) {
                gameWrapper->Execute([this](GameWrapper * gw) { CHECK_NOW(); });
        }

        if (ImGui::Checkbox("SHOW ALL?", &show_all)) {
                if (show_all) {
                        slot1 = slot2 = slot3 = slot4 = slot5 = slot6 = true;
                } else {
                        slot1 = slot2 = slot3 = slot4 = slot5 = slot6 = false;
                }
        }

        if (ImGui::Checkbox("show window stats", &curiouser)) {
                if (curiouser) {
                        showstats = true;
                } else {
                        showstats = false;
                }
        }

        /*
         * On another note, a button that "snapshots" where things *are placed on the
         * overlay, and checkboxes that enable disable movement of windows.
         */
        if (ImGui::Button("SNAPSHOT PLAYLIST NUMBER POSITIONS")) {
                SNAPSHOT_PLAYLIST_POSITIONS();
        }

        ImGui::Text("%d hours selected. (%d days, %d hours)", hours_kept, hours_kept / 24, hours_kept % 24);
        ImGui::SameLine(100.0f, 200.0f);
        if (ImGui::Button("PRUNE DATA FILE?")) {
                // prune the data file
                massage_data();
        }

        // possibly just put this in a range of values? int values[] = {corresponding amount
        // of hours / days} under the hood, it could still just be a flat number... the
        // purpose would be for being "easily readable"... fk it for now
        CVarWrapper hrs_cvar = cvarManager->getCvar(cmd_prefix + "hours");
        int         hrs      = hrs_cvar.getIntValue();
        ImGui::SliderScalar(
                "How long should data be kept(hours)",
                ImGuiDataType_U8,
                &hrs,
                &hours_min,
                &hours_max,
                "%d");
        hrs_cvar.setValue(hrs);

        center_imgui_text("DISCLAIMER:  THE FOLLOWING IS ONLY BASED ON VALUES THAT HAVE "
                          "BEEN SAVED LOCALLY");
        if (ImGui::CollapsingHeader("Data")) {
                ImGui::TextUnformatted("Double-click on plot to re-orient data.");
                // ImPlot
                // can plot lines "time/x-axis" as epoch times, render them as formatted
                ImPlot::BeginPlot(
                        "Population Numbers over Time",
                        "time",
                        "pop",
                        ImVec2(-1, 0),
                        ImPlotFlags_Default,
                        ImPlotAxisFlags_Default | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin,
                        ImPlotAxisFlags_Default | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin);
                ImPlot::EndPlot();
        }
        /*
         * TODO: Add graphing
         *       Add slider ability to select how long data should be kept
         *          ... idk, 8 hours, to 7 days? (7x24 = 148 hours)
         *       ... some tables for min/max/average over that time
         *
         *
         *
         */
}

/// <summary>
/// "SetImGuiContext happens when the plugin’s ImGui is initialized."
/// https://wiki.bakkesplugins.com/imgui/custom_fonts/
///
/// also:
/// "Don't call this yourself, BM will call this function with a pointer
/// to the current ImGui context"
/// ...
///
/// so ¯\(°_o)/¯
/// </summary>
/// <param name="ctx">AFAIK The pointer to the ImGui context</param>
void ShowPlayerPopulation::SetImGuiContext(uintptr_t ctx) {
        ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext *>(ctx));
}

std::string ShowPlayerPopulation::GetPluginName() {
        return "Show Player Population";
}

void ShowPlayerPopulation::add_notifier(
        std::string                                   cmd_name,
        std::function<void(std::vector<std::string>)> do_func,
        std::string                                   desc,
        byte                                          PERMISSIONS = NULL) const {
        cvarManager->registerNotifier(cmd_prefix + cmd_name, do_func, desc, PERMISSIONS);
}

void ShowPlayerPopulation::init_datafile() {
        if (!std::filesystem::exists(gameWrapper->GetDataFolder().append("ShowPlayerPopulation/"))) {
                // create plugin data directory if it doesn't exist
                std::filesystem::create_directory(gameWrapper->GetDataFolder().append("ShowPlayerPopulation/"));
        }

        std::ofstream file {RECORD_POPULATION_FILE, std::ios::app};
        if (!file.good()) {
                throw std::filesystem::filesystem_error("DATA FILE NOT GOOD! UNRECOVERABLE!~", std::error_code());
        }
        if (!std::filesystem::exists(RECORD_POPULATION_FILE) || std::filesystem::is_empty(RECORD_POPULATION_FILE)) {
                csv::CSVWriter<std::ofstream> recordwriter {file};
                std::vector<std::string>      header {"DATETIME", "TOTALPOPULATION", "TOTALPLAYERSONLINE"};

                // EASILY COPY THE HEADERS OVER
                auto vv = std::views::values(bmhelper::playlist_ids_str);
                std::copy(begin(vv), end(vv), std::back_inserter(header));
                recordwriter << header;
        } else {
                csv::CSVReader recordreader {RECORD_POPULATION_FILE.string()};
                csv::CSVRow    row;

                // go to last row
                while (recordreader.read_row(row))
                        ;
                // last_time =
                // row["DATETIME"].get<std::chrono::time_point<std::chrono::system_clock>>();
                std::string tmpdt = row["DATETIME"].get<std::string>();
                last_time         = get_timepoint_from_str(tmpdt);
        }

        GET_DEFAULT_POP_NUMBER_PLACEMENTS();
}

void ShowPlayerPopulation::GET_DEFAULT_POP_NUMBER_PLACEMENTS() {
        // init resolutions
        std::ifstream res_file {POP_NUMBER_PLACEMENTS_FILE};
        // format will be [2560x1440] x1 y1 x2 y2 x3 y3 x4 y4 x5 y5 x6 y6
        int           dispx, dispy;
        int           x1, y1;
        int           x2, y2;
        int           x3, y3;
        int           x4, y4;
        int           x5, y5;
        int           x6, y6;
        std::string   input;
        Vector2       screensize = gameWrapper->GetScreenSize();
        if (screensize.X == -1 && screensize.Y == -1) {
                ImVec2 dsps  = ImGui::GetIO().DisplaySize;
                screensize.X = dsps.x;
                screensize.Y = dsps.y;
        }
        while (std::getline(res_file, input)) {
                std::sscanf(
                        input.c_str(),
                        "[%dx%d] %d %d %d %d %d %d %d %d %d %d %d %d",
                        &dispx,
                        &dispy,
                        &x1,
                        &y1,
                        &x2,
                        &y2,
                        &x3,
                        &y3,
                        &x4,
                        &y4,
                        &x5,
                        &y5,
                        &x6,
                        &y6);
                if (screensize.X == dispx && screensize.Y == dispy) {
                        slot1_init_pos.x = static_cast<float>(x1), slot1_init_pos.y = static_cast<float>(y1);
                        slot2_init_pos.x = static_cast<float>(x2), slot2_init_pos.y = static_cast<float>(y2);
                        slot3_init_pos.x = static_cast<float>(x3), slot3_init_pos.y = static_cast<float>(y3);
                        slot4_init_pos.x = static_cast<float>(x4), slot4_init_pos.y = static_cast<float>(y4);
                        slot5_init_pos.x = static_cast<float>(x5), slot5_init_pos.y = static_cast<float>(y5);
                        slot6_init_pos.x = static_cast<float>(x6), slot6_init_pos.y = static_cast<float>(y6);
                }
        }

        // TRY TO "SCALE"!
        const float final_scale  = gameWrapper->GetDisplayScale() * gameWrapper->GetInterfaceScale();
        slot1_init_pos          *= final_scale;
        slot2_init_pos          *= final_scale;
        slot3_init_pos          *= final_scale;
        slot4_init_pos          *= final_scale;
        slot5_init_pos          *= final_scale;
        slot6_init_pos          *= final_scale;
}

void ShowPlayerPopulation::CHECK_NOW() {
        using namespace std::chrono_literals;
        std::chrono::zoned_time clk {std::chrono::current_zone(), std::chrono::system_clock::now()};
        if ((clk.get_local_time() - last_time.get_local_time()) < 5min) {
                // I wanted to have some sort of "timeout" to basically keep people from
                // spamming their shit?
                LOG("TOO SOON!");
                // return;
        }

        gameWrapper->SetTimeout(
                [this](GameWrapper * gw) {
                        MatchmakingWrapper mw = gw->GetMatchmakingWrapper();
                        if (mw) {
                                mw.StartMatchmaking(PlaylistCategory::CASUAL);
                                mw.CancelMatchmaking();
                        }
                },
                2.0f);
}

void ShowPlayerPopulation::write_population() {
        std::ofstream                 file {RECORD_POPULATION_FILE, std::ios::app};
        csv::CSVWriter<std::ofstream> recordwriter {file};
        MatchmakingWrapper            mw = gameWrapper->GetMatchmakingWrapper();

        playlist_population.clear();

        if (mw) {
                std::vector<int> counts;
                counts.push_back(mw.GetTotalPopulation());
                counts.push_back(mw.GetTotalPlayersOnline());
                for (const auto & element : bmhelper::playlist_ids_str) {
                        int playlist_pop = mw.GetPlayerCount(static_cast<PlaylistIds>(element.first));
                        counts.push_back(playlist_pop);
                        if (playlist_pop > 0) {
                                playlist_population[element.first] = playlist_pop;
                        }
                }

                std::vector<std::string> strs;
                strs.reserve(bmhelper::playlist_ids_str.size() + 1);
                last_time = std::chrono::system_clock::now();
                strs.push_back(get_current_datetime_str());
                std::transform(
                        begin(counts),
                        end(counts),
                        std::back_inserter(strs),
                        [](const auto & elem) -> std::string { return std::to_string(elem); });

                recordwriter << strs;
        }
        massage_data();
}

/// <summary>
/// OOH LA LA
///
/// put the data in a representable format.
///
/// ... if it's necessary
/// </summary>
void ShowPlayerPopulation::massage_data() {
        // prunejabi

        // prepare it to be shown
        std::ifstream  file {RECORD_POPULATION_FILE};
        csv::CSVReader recordreader {file};
}

/// <summary>
/// RETURNS THE DATETIME AS A STRING!
/// </summary>
/// <returns>_now_ represented as a datetime string</returns>
std::string ShowPlayerPopulation::get_current_datetime_str() {
        const std::chrono::zoned_time t {std::chrono::current_zone(), std::chrono::system_clock::now()};
        return std::vformat(DATETIME_FORMAT_STR, std::make_format_args(t));
}

/// <summary>
/// RETURNS THE TIMEPOINT AS INTERPRETED FROM A DATETIME STRING!
/// </summary>
/// <param name="str">a string representation of the datetime</param>
/// <returns>a time_point representing the string</returns>
std::chrono::time_point<std::chrono::system_clock> ShowPlayerPopulation::get_timepoint_from_str(std::string str) {
        std::chrono::utc_time<std::chrono::system_clock::duration> tmpd;
        std::istringstream                                         ss(str);
        ss >> std::chrono::parse(DATETIME_PARSE_STR, tmpd);
        std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> tmptp {
                tmpd.time_since_epoch()};
        return tmptp;
}

void ShowPlayerPopulation::SET_WHICH_MENU_I_AM_IN() {
        // clear flags
        in_main_menu = in_playlist_menu = in_game_menu = false;

        // check what menu we're in when this gets triggered
        MenuStackWrapper msw = gameWrapper->GetMenuStack();
        if (msw) {
                std::string menu_name = msw.GetTopMenu();
                if (menu_name == "RootMenuMovie") {
                        LOG("IN MAIN MENU");
                        in_main_menu = true;
                } else if (menu_name == "PlayMenuV4Movie") {
                        LOG("IN PLAYLIST MENU");
                        in_playlist_menu = true;
                } else if (menu_name == "MidGameMenuMovie") {
                        LOG("IN GAME MENU");
                        in_game_menu = true;
                }
        }
}

/*
 * for when you've inherited from BakkesMod::Plugin::PluginWindow.
 * this lets  you do "togglemenu (GetMenuName())" in BakkesMod's console...
 * ie
 * if the following GetMenuName() returns "xyz", then you can refer to your
 * plugin's window in game through "togglemenu xyz"
 */

/// <summary>
/// do the following on togglemenu open
/// </summary>
void ShowPlayerPopulation::OnOpen() {
        is_overlay_open = true;
};

/// <summary>
/// do the following on menu close
/// </summary>
void ShowPlayerPopulation::OnClose() {
        is_overlay_open = false;
};

/// <summary>
/// https://mastodon.gamedev.place/@dougbinks/99009293355650878
/// </summary>
void ShowPlayerPopulation::add_underline(ImColor col_) {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        min.y      = max.y;
        ImGui::GetWindowDrawList()->AddLine(min, max, col_, 1.0f);
}

/// <summary>
/// inspiration:
/// https://stackoverflow.com/questions/64653747/how-to-center-align-text-horizontally
/// </summary>
/// <param name="text">The text to center.</param>
void ShowPlayerPopulation::center_imgui_text(const std::string & text) {
        // calc width so far.
        int   cur_col            = ImGui::GetColumnIndex();
        float total_width_so_far = 0.0f;
        while (--cur_col >= 0) {
                // get each column width, down to 0, break at -1
                total_width_so_far += ImGui::GetColumnWidth(cur_col);
        }

        float width      = ImGui::GetContentRegionAvailWidth();
        float text_width = ImGui::CalcTextSize(text.c_str()).x;

        float indent = (width - text_width) * 0.5f;

        float min_indent = 0.0f;
        if (std::fabs(indent - min_indent) <= 1e-6) {
                indent = min_indent;
        }

        ImGui::SetCursorPosX(total_width_so_far + indent);
        ImGui::TextUnformatted(text.c_str());
}

/// <summary>
/// (ImGui) Code called while rendering your menu window
/// </summary>
void ShowPlayerPopulation::Render() {
        if (in_main_menu || in_game_menu) {
                // SHOW THE DAMN NUMBERS, JIM!
                ImGui::SetNextWindowSize(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::Begin("Hey, cutie", NULL);
                ImGui::SetWindowFontScale(1.2f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                center_imgui_text(
                        std::vformat("POPULATIONS! LAST UPDATED: {0:%r} {0:%D}", std::make_format_args(last_time))
                                .c_str());
                ImGui::NewLine();
                ImGui::Indent(20.0f);
                ImGui::BeginColumns("totalpopcol", 2, ImGuiColumnsFlags_NoResize);
                ImGui::TextUnformatted("Total Players Online:");
                ImGui::NextColumn();
                center_imgui_text(std::to_string(TOTAL_POP));
                ImGui::NextColumn();
                ImGui::EndColumns();

                if (ImGui::GetWindowWidth() <= (ImGui::GetIO().DisplaySize.x / 2.0f)) {
                        // less than or equal to half of the width of the screen
                        // = "vertical layout"
                        ImGui::BeginColumns("populationnums_vert", 2, ImGuiColumnsFlags_NoResize);
                        for (const std::string playlists : SHOWN_PLAYLIST_POPS) {
                                for (const PlaylistId & id : bmhelper::PlaylistCategories[playlists]) {
                                        std::string playliststr = bmhelper::playlist_ids_str_spaced[id];
                                        int         pop         = playlist_population[id];
                                        if (pop < 1) {
                                                continue;
                                        }
                                        ImGui::TextUnformatted(
                                                std::vformat("{}:", std::make_format_args(playliststr)).c_str());
                                        ImGui::NextColumn();
                                        center_imgui_text(std::vformat("{}", std::make_format_args(pop)));
                                        ImGui::NextColumn();
                                }
                                ImGui::NewLine();
                                ImGui::NextColumn();
                                ImGui::NewLine();
                                ImGui::NextColumn();
                        }
                        ImGui::EndColumns();
                } else {
                        // greater than half of the width of the screen
                        // "horizontal layout
                        size_t mx = 0;

                        // get max number of lines
                        for (const auto & neat : SHOWN_PLAYLIST_POPS) {
                                mx = std::max(bmhelper::PlaylistCategories[neat].size(), mx);
                        }

                        // try to write out each one
                        // 12 ... because 6 playlists, 6 numbers

                        //////// fucking preprocess -_-
                        ///////
                        std::map<std::string, std::vector<std::pair<int, int>>> pops_horiz;
                        // im so tired
                        ImGui::BeginColumns("populationnums_horiz", 12, ImGuiColumnsFlags_NoResize);
                        for (int line = 0; line < mx; ++line) {
                                for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                                        if (line >= bmhelper::PlaylistCategories[playstr].size()) {
                                                ImGui::NextColumn();
                                                ImGui::NextColumn();
                                                continue;
                                        } else {
                                                PlaylistId id  = bmhelper::PlaylistCategories[playstr][line];
                                                int        pop = playlist_population[id];
                                                if (pop < 1) {
                                                        ImGui::NextColumn();
                                                        ImGui::NextColumn();
                                                        continue;
                                                }
                                                ImGui::TextUnformatted(
                                                        std::vformat(
                                                                "{}:",
                                                                std::make_format_args(
                                                                        bmhelper::playlist_ids_str_spaced[id]))
                                                                .c_str());
                                                ImGui::NextColumn();
                                                center_imgui_text(std::vformat("{}", std::make_format_args(pop)));
                                                ImGui::NextColumn();
                                        }
                                }
                        }
                        ImGui::EndColumns();
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::End();
        } else if (in_playlist_menu) {
        }
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImVec2      onebar, twobar, threebar, fourbar, fivebar, sixbar;
        const float WIN_HEIGHT = 40.0f;
        const float WIN_WIDTH  = 247.0f;
        // THE ImGuiCond_Always ON POSITIONS RIGHT NOW IS FOR TESTING!!!!!!!!!!!
        // LATER THEY WILL BE ImGuiCond_FirstUseEver
        if (slot1) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot1_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show1", &slot1, ImGuiWindowFlags_NoTitleBar);
                onepos = ImGui::GetWindowPos();
                onebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot2) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot2_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show2", &slot2, ImGuiWindowFlags_NoTitleBar);
                twopos = ImGui::GetWindowPos();
                twobar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot3) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot3_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show3", &slot3, ImGuiWindowFlags_NoTitleBar);
                threepos = ImGui::GetWindowPos();
                threebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot4) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot4_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show4", &slot4, ImGuiWindowFlags_NoTitleBar);
                fourpos = ImGui::GetWindowPos();
                fourbar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot5) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot5_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show5", &slot5, ImGuiWindowFlags_NoTitleBar);
                fivepos = ImGui::GetWindowPos();
                fivebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot6) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(slot6_init_pos, ImGuiCond_FirstUseEver);
                ImGui::Begin("show6", &slot6, ImGuiWindowFlags_NoTitleBar);
                sixpos = ImGui::GetWindowPos();
                sixbar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (showstats) {
                ImGui::SetNextWindowSize(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(70, 70), ImGuiCond_FirstUseEver);
                ImGui::Begin("stats", NULL);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                center_imgui_text(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(onepos.x, onepos.y, onebar.x, onebar.y)));
                center_imgui_text(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(twopos.x, twopos.y, twobar.x, twobar.y)));
                center_imgui_text(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(threepos.x, threepos.y, threebar.x, threebar.y)));
                center_imgui_text(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(fourpos.x, fourpos.y, fourbar.x, fourbar.y)));
                center_imgui_text(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(fivepos.x, fivepos.y, fivebar.x, fivebar.y)));
                center_imgui_text(std::vformat(
                        "SHOW1| X: {} . Y: {} | WIDTH: {} . HEIGHT: {}",
                        std::make_format_args(sixpos.x, sixpos.y, sixbar.x, sixbar.y)));
                ImGui::End();
                ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor();
};

/// <summary>
/// Returns the name of the menu to refer to it by
/// </summary>
/// <returns>The name used refered to by togglemenu</returns>
std::string ShowPlayerPopulation::GetMenuName() {
        return "ShowPlayerPopulation";
};

/// <summary>
/// Returns a std::string to show as the title
/// </summary>
/// <returns>The title of the menu</returns>
std::string ShowPlayerPopulation::GetMenuTitle() {
        return "";
};

/// <summary>
/// Is it the active overlay(window)?
/// Might make this susceptible to being closed with Esc
/// Makes it "active", aka, able to be manipulated when the plugin menu is closed.
/// </summary>
/// <returns>True/False for being the active overlay</returns>
bool ShowPlayerPopulation::IsActiveOverlay() {
        return false;
};

/// <summary>
/// Should this block input from the rest of the program?
/// (aka RocketLeague and BakkesMod windows)
/// </summary>
/// <returns>True/False for if bakkesmod should block input. Could be
/// `return ImGui::GetIO().WantCaptureMouse ||
/// ImGui::GetIO().WantCaptureKeyboard;`</returns>
bool ShowPlayerPopulation::ShouldBlockInput() {
        return false;
};
