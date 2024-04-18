#include "ShowPlayerPopulation.h"
#include <format>
#include <fstream>
#include <ranges>

#include "bakkesmod/imgui/imgui.h"
#include "bakkesmod/imgui/imgui_internal.h"

#include "csv.hpp"
#include "implot.h"

#include "bmhelper.h"
#include "HookedEvents.h"
#include "Logger.h"

const std::string ShowPlayerPopulation::cmd_prefix = "rppd_";

BAKKESMOD_PLUGIN(ShowPlayerPopulation, "ShowPlayerPopulation", "0.7.8", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/// <summary>
/// do the following when your plugin is loaded
/// </summary>
void ShowPlayerPopulation::onLoad() {
        // initialize things
        _globalCvarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        init_datafile();

        // so tired...
        LOG("{}", last_time);

        using namespace std::chrono_literals;
        std::chrono::zoned_time clk {
                std::chrono::current_zone(),
                std::chrono::system_clock::now()};
        if ((clk.get_local_time() - last_time.get_local_time()) < 5min) {
                // I wanted to have some sort of "timeout" to basically keep people from
                // spamming their shit?
                LOG("TOO SOON!");
        }
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

        show_overlay_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) {
                        if (is_overlay_open != new_value.getBoolValue()) {
                                gameWrapper->Execute([this](GameWrapper * gw) {
                                        // look, sometimes cvarManager just gets fucking
                                        // lost.
                                        cvarManager->executeCommand(
                                                "togglemenu ShowPlayerPopulation",
                                                false);
                                });
                        }
                });

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
                [this](std::string eventName) {
                        // conveniently stops the playlist population query
                });
        HookedEvents::AddHookedEvent(
                "Function ProjectX.PsyNetConnection_X.EventConnected",
                [this](std::string eventName) {
                        // conveniently is called whenever you connect to psynet
                        // which puts you in a great state to "StartMatchingmaking" / query
                        // matchmaking

                        CHECK_NOW();
                });

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

void SNAPSHOT_PLAYLIST_POSITIONS() {
}
/// <summary>
/// This call usually includes ImGui code that is shown and rendered (repeatedly,
/// on every frame rendered) when your plugin is selected in the plugin
/// manager. AFAIK, if your plugin doesn't have an associated *.set file for its
/// settings, this will be used instead.
/// </summary>
void ShowPlayerPopulation::RenderSettings() {
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

        if (ImGui::Button("SNAPSHOT PLAYLIST NUMBER POSITIONS")) {
                SNAPSHOT_PLAYLIST_POSITIONS();
        }

        ImGui::Text(
                "%d hours selected. (%d days, %d hours)",
                hours_kept,
                hours_kept / 24,
                hours_kept % 24);
        ImGui::SameLine(0.0f, 100.0f);
        if (ImGui::Button("PRUNE DATA FILE?")) {
                // prune the data file
        }

        // possibly just put this in a range of values?
        ImGui::SliderScalar(
                "How long should data be kept(hours)",
                ImGuiDataType_U8,
                &hours_kept,
                &hours_min,
                &hours_max,
                "%d");
        /*
         * TODO: Add graphing
         *       Add slider ability to select how long data should be kept
         *          ... idk, 8 hours, to 7 days? (7x24 = 148 hours)
         *       ... some tables for min/max/average over that time
         *       a disclaimer definitely stating how the following is only based
         *       on values that have been saved locally
         *
         *       On another note, a button that "snapshots" where things
         *       are placed on the overlay, and checkboxes that enable/disable
         *       movement of windows.
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
        std::ofstream file {RECORD_POPULATION_FILE, std::ios::app};
        if (!file.good()) {
                throw std::filesystem::filesystem_error(
                        "DATA FILE NOT GOOD! UNRECOVERABLE!~",
                        std::error_code());
        }
        if (!std::filesystem::exists(RECORD_POPULATION_FILE)
            || std::filesystem::is_empty(RECORD_POPULATION_FILE)) {
                csv::CSVWriter<std::ofstream> recordwriter {file};
                std::vector<std::string>      header {
                        "DATETIME",
                        "TOTALPOPULATION",
                        "TOTALPLAYERSONLINE"};

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
}

void ShowPlayerPopulation::CHECK_NOW() {
        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
        if (mw) {
                mw.StartMatchmaking(PlaylistCategory::CASUAL);
                mw.CancelMatchmaking();
        }
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
                playlist_population.push_back({"Total Players Online", counts.back()});
                for (const auto & element : bmhelper::playlist_ids_str) {
                        counts.push_back(mw.GetPlayerCount(element.first));
                        if (int n = counts.back(); n > 0) {
                                playlist_population.push_back(
                                        {std::vformat(
                                                 "{}",
                                                 std::make_format_args(
                                                         bmhelper::playlist_ids_str_spaced
                                                                 [element.first])),
                                         n});
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
                        [](const auto & elem) -> std::string {
                                return std::to_string(elem);
                        });

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
}

/// <summary>
/// RETURNS THE DATETIME AS A STRING!
/// </summary>
/// <returns>_now_ represented as a datetime string</returns>
std::string ShowPlayerPopulation::get_current_datetime_str() {
        const std::chrono::zoned_time t {
                std::chrono::current_zone(),
                std::chrono::system_clock::now()};
        return std::vformat(DATETIME_FORMAT_STR, std::make_format_args(t));
}

/// <summary>
/// RETURNS THE TIMEPOINT AS INTERPRETED FROM A DATETIME STRING!
/// </summary>
/// <param name="str">a string representation of the datetime</param>
/// <returns>a time_point representing the string</returns>
std::chrono::time_point<std::chrono::system_clock>
        ShowPlayerPopulation::get_timepoint_from_str(std::string str) {
        std::chrono::utc_time<std::chrono::system_clock::duration> tmpd;
        std::istringstream                                         ss(str);
        ss >> std::chrono::parse(DATETIME_PARSE_STR, tmpd);
        std::chrono::
                time_point<std::chrono::system_clock, std::chrono::system_clock::duration>
                        tmptp {tmpd.time_since_epoch()};
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
                center_imgui_text(std::vformat(
                                          "POPULATIONS! LAST UPDATED: {0:%r} {0:%D}",
                                          std::make_format_args(last_time))
                                          .c_str());
                ImGui::NewLine();
                ImGui::Indent(20.0f);
                ImGui::BeginColumns("populationnums", 2, ImGuiColumnsFlags_NoResize);
                // ImGui::colum
                for (const std::pair<std::string, int> & playlist : playlist_population) {
                        ImGui::TextUnformatted(
                                std::vformat("{}:", std::make_format_args(playlist.first))
                                        .c_str());
                        ImGui::NextColumn();
                        center_imgui_text(
                                std::vformat("{}", std::make_format_args(playlist.second)));
                        ImGui::NextColumn();
                }
                ImGui::EndColumns();
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::End();
        } else if (in_playlist_menu) {
        }
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImVec2      onepos, twopos, threepos, fourpos, fivepos, sixpos;
        ImVec2      onebar, twobar, threebar, fourbar, fivebar, sixbar;
        float       topX, topY, botX, botY;
        const float WIN_HEIGHT = 40.0f;
        const float WIN_WIDTH  = 247.0f;
        topX                   = (ImGui::GetIO().DisplaySize.x / 7.111) + (588 - WIN_WIDTH);
        topY                   = (ImGui::GetIO().DisplaySize.y / 4.817) - WIN_HEIGHT;
        botX                   = (ImGui::GetIO().DisplaySize.x / 5.688) + (530 - WIN_WIDTH);
        botY                   = (ImGui::GetIO().DisplaySize.y / 2.051) - WIN_HEIGHT;
        if (slot1) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(topX, topY), ImGuiCond_Always);
                ImGui::Begin("show1", &slot1, ImGuiWindowFlags_NoTitleBar);
                onepos = ImGui::GetWindowPos();
                ImGui::SetWindowPos(ImVec2(
                        std::max(0.0f, std::min(onepos.x, ImGui::GetIO().DisplaySize.x)),
                        std::max(0.0f, std::min(onepos.y, ImGui::GetIO().DisplaySize.y))));
                onebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot2) {
                ImGui::SetNextWindowSize(ImVec2(WIN_WIDTH, WIN_HEIGHT), ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(botX, botY), ImGuiCond_Always);
                ImGui::Begin("show2", &slot2, ImGuiWindowFlags_NoTitleBar);
                twopos = ImGui::GetWindowPos();
                twobar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot3) {
                ImGui::SetNextWindowSize(
                        ImVec2(WIN_WIDTH, WIN_HEIGHT),
                        ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(30, 30), ImGuiCond_FirstUseEver);
                ImGui::Begin("show3", &slot3, ImGuiWindowFlags_NoTitleBar);
                threepos = ImGui::GetWindowPos();
                threebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot4) {
                ImGui::SetNextWindowSize(
                        ImVec2(WIN_WIDTH, WIN_HEIGHT),
                        ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(
                        ImVec2(WIN_HEIGHT, WIN_HEIGHT),
                        ImGuiCond_FirstUseEver);
                ImGui::Begin("show4", &slot4, ImGuiWindowFlags_NoTitleBar);
                fourpos = ImGui::GetWindowPos();
                fourbar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot5) {
                ImGui::SetNextWindowSize(
                        ImVec2(WIN_WIDTH, WIN_HEIGHT),
                        ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
                ImGui::Begin("show5", &slot5, ImGuiWindowFlags_NoTitleBar);
                fivepos = ImGui::GetWindowPos();
                fivebar = ImGui::GetWindowSize();
                ImGui::End();
        }
        if (slot6) {
                ImGui::SetNextWindowSize(
                        ImVec2(WIN_WIDTH, WIN_HEIGHT),
                        ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
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
                        std::make_format_args(
                                threepos.x,
                                threepos.y,
                                threebar.x,
                                threebar.y)));
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
