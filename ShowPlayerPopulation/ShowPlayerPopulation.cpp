#include "ShowPlayerPopulation.h"
#include <format>
#include <fstream>
#include <ranges>

#include "bakkesmod/imgui/imgui_internal.h"

#include "csv.hpp"
#include "implot.h"

#include "HookedEvents.h"
#include "internal/csv_row.hpp"
#include "Logger.h"

/*
 *
 *  - cleanup code
 * todo: alpha release:
 *  - make pruning work
 *    - doesn't work because of the bank possibly being empty, lol
 *  - graph
 *  - take out superfluous options used for testing
 *       .   make a new branch to clean up for "release"
 *
 */

const std::string ShowPlayerPopulation::cmd_prefix = "spp_";

BAKKESMOD_PLUGIN(ShowPlayerPopulation, "ShowPlayerPopulation", "0.68.8", /*UNUSED*/ NULL);
std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

/// <summary>
/// do the following when your plugin is loaded
///
/// is cvar manager only like, guaranteed in here? what the fuck??
/// </summary>
void ShowPlayerPopulation::onLoad() {
        // initialize things
        _globalCvarManager        = cvarManager;
        HookedEvents::gameWrapper = gameWrapper;

        // ImGuiIO & io = ImGui::GetIO();
        // overlay_font_22 = io.Fonts->AddFontFromFileTTF(
        //         (gameWrapper->GetDataFolder().append("fonts/SourceSerifPro-Black.ttf")).string().c_str(),
        //         22.0f);
        // overlay_font_18 = io.Fonts->AddFontFromFileTTF(
        //         (gameWrapper->GetDataFolder().append("fonts/SourceSerifPro-Black.ttf")).string().c_str(),
        //         18.0f);

        init_datafile();
        init_cvars();
        init_hooked_events();
        SET_WHICH_MENU_I_AM_IN();
}

/// <summary>
/// This will house initialization code for all the cvars used for the plugin.
/// </summary>
void ShowPlayerPopulation::init_cvars() {
        // cvarManager->registerCvar(
        //         cmd_prefix + "enable_topscroll",
        //         "0",
        //         "Flag to determine showing the top ticker scroll showing player
        //         population");

        CVarWrapper show_overlay_cvar = cvarManager->registerCvar(
                cmd_prefix + "flag_show_overlay",
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

        CVarWrapper lock_overlay_cv =
                cvarManager->registerCvar(cmd_prefix + "flag_lock_overlay", "0", "Flag for locking the overlay", false);
        lock_overlay_cv.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { lock_overlay = new_value.getBoolValue(); });

        CVarWrapper show_menu_cv = cvarManager->registerCvar(
                cmd_prefix + "flag_show_in_menu",
                "0",
                "Flag for showing the overlay in the main menu",
                false);
        show_menu_cv.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { show_in_main_menu = new_value.getBoolValue(); });

        CVarWrapper show_playlist_menu_cv = cvarManager->registerCvar(
                cmd_prefix + "flag_show_in_playlist_menu",
                "0",
                "Flag for showing the overlay during playlist selection",
                false);
        show_playlist_menu_cv.addOnValueChanged([this](std::string old_value, CVarWrapper new_value) {
                show_in_playlist_menu = new_value.getBoolValue();
        });

        CVarWrapper show_game_menu_cv = cvarManager->registerCvar(
                cmd_prefix + "flag_show_in_game_menu",
                "0",
                "Flag for showing the overlay in game while paused",
                false);
        show_game_menu_cv.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { show_in_game_menu = new_value.getBoolValue(); });

        CVarWrapper hrs_cvar = cvarManager->registerCvar(
                cmd_prefix + "hours_kept",
                "24",
                "Number of hours to keep of population data.",
                false);
        hrs_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { hours_kept = new_value.getIntValue(); });

        CVarWrapper keep_cvar = cvarManager->registerCvar(
                cmd_prefix + "keep_indefinitely",
                "0",
                "Should data be kept indefinitely?",
                false);
        keep_cvar.addOnValueChanged(
                [this](std::string old_value, CVarWrapper new_value) { keep_all_data = new_value.getBoolValue(); });
}

/// <summary>
/// This will house initialization code for all the hooked events
/// </summary>
void ShowPlayerPopulation::init_hooked_events() {
        HookedEvents::AddHookedEvent(
                "Function ProjectX.OnlineGamePopulation_X.GetPlaylistPopulations",
                // conveniently stops after disconnected
                [this](std::string ev) {
                        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
                        if (mw) {
                                int tmpp = mw.GetTotalPlayersOnline();
                                if (get_last_entry().total_pop != tmpp) {
                                        record_population();
                                        prune_data();
                                        prepare_data();
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
                [this](std::string eventName) { DO_CHECK = true; });

        HookedEvents::AddHookedEvent(
                "Function TAGame.StatusObserver_MenuStack_TA.HandleMenuChange",
                [this](std::string eventName) {
                        SET_WHICH_MENU_I_AM_IN();
                        if (in_main_menu && DO_CHECK) {
                                // roundabout way to try and avoid the "set playlist" error.
                                CHECK_NOW();
                                DO_CHECK = false;
                        }
                });
}

/// <summary>
///  do the following when your plugin is unloaded
/// </summary>
void ShowPlayerPopulation::onUnload() {
        // destroy things
        // dont throw here

        prune_data();
        write_data_to_file();
}

/************* NO REDUNDANCY CHECKING FOR DATA IN THIS PROGRAM &&&&&&&&&&&&&&&***********/
void ShowPlayerPopulation::write_data_to_file() {
        std::ifstream            ifile {RECORD_POPULATION_FILE};
        std::vector<std::string> header = csv::CSVReader {ifile}.get_col_names();  // THE ONLY "SANITY" ALLOWED
        ifile.close();

        std::ofstream  ofile {RECORD_POPULATION_FILE};
        csv::CSVWriter recordwriter {ofile};

        recordwriter << header;

        for (const auto & item : bank) {
                std::vector<std::string> data_written;
                data_written.push_back(std::move(std::vformat(DATETIME_FORMAT_STR, std::make_format_args(item.zt))));
                data_written.push_back(std::move(std::to_string(item.total_pop)));
                data_written.push_back(std::move(std::to_string(item.total_players_online)));
                for (const auto & col : header) {
                        if (bmhelper::playlist_str_ids.contains(col)) {
                                PlaylistId playid = bmhelper::playlist_str_ids[col];
                                data_written.push_back(std::move(std::to_string(item.playlist_pop.at(playid))));
                        }
                }
                recordwriter << data_written;
        }
        ofile.close();
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

void ShowPlayerPopulation::print_bank_info() {
        LOG("NUM ENTRIES IN BANK: {}", bank.size());
        LOG("HOURS BETWEEN LAST AND FIRST: {:%H}",
            get_last_entry().zt.get_local_time() - get_first_entry().zt.get_local_time());
}

/// <summary>
/// This is for helping with IMGUI stuff
///
/// I don't know why center_imgui_text was made a member function
///
/// copied from: https://github.com/ocornut/imgui/discussions/3862
/// </summary>
/// <param name="width">total width of items</param>
/// <param name="alignment">where on the line to align</param>
static void AlignForWidth(float width, float alignment = 0.5f) {
        float avail = ImGui::GetContentRegionAvail().x;
        float off   = (avail - width) * alignment;
        if (off > 0.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
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

        CVarWrapper shoverlay  = cvarManager->getCvar(cmd_prefix + "flag_show_overlay");
        bool        bshoverlay = shoverlay.getBoolValue();
        if (ImGui::Checkbox("Show the overlay?", &bshoverlay)) {
                shoverlay.setValue(bshoverlay);
        }

        // show in main menu, show in game, show in playlist menu | flags
        // still need color / transparency /location locking options for the overlays

        if (ImGui::Button("CHECK POPULATION NOW")) {
                gameWrapper->Execute([this](GameWrapper * gw) { CHECK_NOW(); });
        }

        if (ImGui::Checkbox("Lock overlay?", &lock_overlay)) {
                CVarWrapper cvw = cvarManager->getCvar(cmd_prefix + "flag_lock_overlay");
                cvw.setValue(lock_overlay);
        }

        if (ImGui::Checkbox("Show in main menu?", &show_in_main_menu)) {
                CVarWrapper cvw = cvarManager->getCvar(cmd_prefix + "flag_show_in_menu");
                cvw.setValue(show_in_main_menu);
        }
        ImGui::SameLine(0, 50.0f);
        if (ImGui::Checkbox("Show in playlist menu?", &show_in_playlist_menu)) {
                CVarWrapper cvw = cvarManager->getCvar(cmd_prefix + "flag_show_in_playlist_menu");
                cvw.setValue(show_in_playlist_menu);
        }
        ImGui::SameLine(0, 50.0f);
        if (ImGui::Checkbox("Show during pause menu in game?", &show_in_game_menu)) {
                CVarWrapper cvw = cvarManager->getCvar(cmd_prefix + "flag_show_in_game_menu");
                cvw.setValue(show_in_game_menu);
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
        CVarWrapper keep_data_cvar = cvarManager->getCvar(cmd_prefix + "keep_indefinitely");
        bool        bkeep          = keep_data_cvar.getBoolValue();
        if (ImGui::Checkbox("KEEP DATA INDEFINITELY", &bkeep)) {
                keep_data_cvar.setValue(bkeep);
        }
        if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(
                        "IF UNSET, THIS WILL TAKE EFFECT WHEN THE PLUGIN UNLOADS\n (LIKE WHEN THE GAME IS CLOSED)");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
        }

        if (bkeep) {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        ImGui::SameLine(300.0f, 200.0f);
        static bool popup = false;
        if (ImGui::Button("PRUNE DATA FILE?")) {
                popup = true;
                ImGui::OpenPopup("PRUNE_DATA_CONFIRM");
                prepare_data();
        }
        if (ImGui::BeginPopupModal(
                    "PRUNE_DATA_CONFIRM",
                    &popup,
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration)) {
                ImGui::SetWindowSize(ImVec2(300.0f, 80.0f), ImGuiCond_Always);
                center_imgui_text("Are you sure you want to prune your data?");
                ImGuiStyle & style  = ImGui::GetStyle();
                float        width  = 0.0f;
                width              += 80.0f;  // fixed size button 1
                width              += style.ItemSpacing.x;
                width              += 50.0f;  // because of the sameline + spacing
                width              += 80.0f;  // fixed size button 2
                AlignForWidth(width, 0.5f);
                if (ImGui::Button("yes", ImVec2(80, 20))) {
                        ImGui::CloseCurrentPopup();
                        print_bank_info();
                        prune_data();
                }
                ImGui::SameLine(0.0f, 50.0f);
                if (ImGui::Button("no", ImVec2(80, 20))) {
                        write_data_to_file();
                        ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
        }

        CVarWrapper hrs_cvar = cvarManager->getCvar(cmd_prefix + "hours_kept");
        int         hrs      = hrs_cvar.getIntValue();
        ImGui::SliderScalar(
                "How long should data be kept(hours)",
                ImGuiDataType_U8,
                &hrs,
                &hours_min,
                &hours_max,
                "%d");
        hrs = std::max(hours_min, std::min(hours_max, hrs));
        hrs_cvar.setValue(hrs);
        if (bkeep) {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
        }

        ImGui::NewLine();
        center_imgui_text("DISCLAIMER:  THE FOLLOWING IS ONLY BASED ON VALUES THAT HAVE "
                          "BEEN SAVED LOCALLY");

        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.2f, 0.8f, 1.0f));
        if (ImGui::CollapsingHeader("Data")) {
                ImGui::TextUnformatted("Double-click on plot to re-orient data.");
                if (ImPlot::BeginPlot(
                            "Population Numbers over Time",
                            "time",
                            "pop",
                            ImVec2(-1, 0),
                            ImPlotFlags_Default,
                            ImPlotAxisFlags_Default | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin,
                            ImPlotAxisFlags_Default | ImPlotAxisFlags_LockMax | ImPlotAxisFlags_LockMin)) {
                        // can plot lines "time/x-axis" as epoch times, render them as formatted
                        // vertical lines should separate days... that'd be cool
                        // well... brute force for now, I suppose

                        ImPlot::EndPlot();
                }
        }

        // ADD A BIG OL' DISCLAIMER-EXPLANATION DOWN HERE ON HOW THINGS WORK!
        if (ImGui::CollapsingHeader("How does this work?")) {
                // SOME TEXT EXPLAINING HOW THINGS WORK
                ImGui::TextUnformatted("STILL WORKING");
        }

        if (ImGui::CollapsingHeader("Some questions you may have...")) {
                // SOME TEXT EXPLAINING SOME QUESTIONS
                AlignForWidth(ImGui::CalcTextSize("ON THIS").x);
                ImGui::TextUnformatted("ON THIS");
        }
        ImGui::PopStyleColor();
}

/// <summary>
/// "SetImGuiContext happens when the plugins ImGui is initialized."
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
                // a first run of the plugin
                csv::CSVWriter<std::ofstream> recordwriter {file};
                std::vector<std::string>      header {"DATETIME", "TOTALPOPULATION", "TOTALPLAYERSONLINE"};

                // EASILY COPY THE HEADERS OVER
                auto vv = std::views::values(bmhelper::playlist_ids_str);
                std::copy(begin(vv), end(vv), std::back_inserter(header));
                recordwriter << header;
        } else {
                // the data file exists.
                csv::CSVReader recordreader {RECORD_POPULATION_FILE.string()};
                csv::CSVRow    row;

                // read in all rows to be kept in memory
                const std::vector<std::string> header = std::move(recordreader.get_col_names());
                while (recordreader.read_row(row)) {
                        std::map<PlaylistId, int> playlist_pop;
                        for (const auto & str : header) {
                                if (bmhelper::playlist_str_ids.contains(str)) {
                                        playlist_pop[bmhelper::playlist_str_ids[str]] = row[str].get<int>();
                                }
                        }
                        bank.emplace_back(token {
                                .zt =
                                        std::chrono::zoned_time {
                                                                 std::chrono::current_zone(),
                                                                 get_timepoint_from_str(row["DATETIME"].get<std::string>())},
                                .total_pop            = row["TOTALPOPULATION"].get<int>(),
                                .total_players_online = row["TOTALPLAYERSONLINE"].get<int>(),
                                .playlist_pop         = playlist_pop
                        });
                }
                std::sort(begin(bank), end(bank), [](const token & t1, const token & t2) {
                        return t1.zt.get_local_time().time_since_epoch() < t2.zt.get_local_time().time_since_epoch();
                });
        }

        prepare_data();
        // not working,yet
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
        if ((clk.get_local_time() - get_last_entry().zt.get_local_time()) < 5min) {
                // I wanted to have some sort of "timeout" to basically keep people from
                // spamming their shit?
                LOG("TOO SOON!");
                // return;
        }

        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();
        if (mw) {
                // EXTRAS because (I believe) it gets around the "Please select a playlist" notification popping up...
                // maybe since there's no "EXTRAS" playlist category anymore? ... When I had this at casual, it was
                // causing an annoying error popup sometimes.
                mw.StartMatchmaking(PlaylistCategory::EXTRAS);
                mw.CancelMatchmaking();
        }
}

void ShowPlayerPopulation::record_population() {
        MatchmakingWrapper mw = gameWrapper->GetMatchmakingWrapper();

        if (mw) {
                token entry;
                entry.total_pop            = mw.GetTotalPopulation();
                entry.total_players_online = mw.GetTotalPlayersOnline();
                for (const auto & element : bmhelper::playlist_ids_str) {
                        entry.playlist_pop[element.first] = mw.GetPlayerCount(static_cast<PlaylistIds>(element.first));
                }
                entry.zt = std::chrono::zoned_time {
                        std::chrono::current_zone(),
                        get_timepoint_from_str(get_current_datetime_str())};
                bank.push_back(std::move(entry));
        }
}

/// <summary>
/// OOH LA LA
///
/// put the data in a representable format.
///
/// </summary>
void ShowPlayerPopulation::prepare_data() {
        // prepare it to be shown for graphing purposes
        // ... this should be for all purposes really o, o}

        const std::map<PlaylistId, int> & playlist_population = get_last_entry().playlist_pop;

        // prepare data to be shown for when the window goes "horizontal"

        // clear persistent data :}
        population_data.clear();
        TOTAL_IN_GAME_POP = 0;

        // get max number of lines
        size_t mxlines = 0;
        for (const auto & neat : SHOWN_PLAYLIST_POPS) {
                mxlines = std::max(bmhelper::playlist_categories[neat].size(), mxlines);
        }

        // put those lines into a data structure
        // (this is solely done to avoid empty lines in the overlay display.)
        for (int line = 0; line < mxlines; ++line) {
                for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                        if (line >= bmhelper::playlist_categories[playstr].size()) {
                                continue;
                        } else {
                                PlaylistId id  = bmhelper::playlist_categories[playstr][line];
                                int        pop = 0;
                                if (playlist_population.contains(id)) {
                                        pop = playlist_population.at(id);
                                }
                                if (pop < 10) {
                                        // Why less than 10?
                                        // Developers tend to jump into modes at their whim
                                        // The population numbers are still tracked no matter who is in the
                                        // mode, so you would see 1 or 2 people in a mode that isn't available
                                        // to the public. I'm guessing, if they don't want to show population
                                        // data to begin with, maybe they don't want people stalking those
                                        // people? like, "why are they blah blah when blah blah?" ... idk. It
                                        // just doesn't seem very relevant to track the 1 or 2 or 9 people
                                        // playing in an inaccessible mode. .... except FaceIt for some reason.

                                        // and 10 ... because... to give an opportunity to catch enough people
                                        // in the custom training editor
                                        continue;
                                }
                                population_data[playstr].push_back({id, pop});
                                TOTAL_IN_GAME_POP += pop;
                        }
                }
        }
}

/// <summary>
/// Puts the data in a state that adheres to specifications
/// Namely that all recorded events fit within a certain timeframe.
/// </summary>
void ShowPlayerPopulation::prune_data() {
        if (keep_all_data) {
                return;
        }

        if (hours_kept == 0) {
                bank.clear();
        }

        // since the list is sorted, pop off the top
        using namespace std::chrono_literals;
        std::chrono::zoned_time yt {std::chrono::current_zone(), std::chrono::system_clock::now()};
        std::chrono::hours      h {hours_kept};

        // get_first_entry or bank.front() works here due to shortcutting when asking bank.empty()
        while (!bank.empty() && (yt.get_local_time() - bank.front().zt.get_local_time()) > h) {
                bank.pop_front();
        }
}

/// <summary>
/// WHY? BECAUSE .front() ON AN EMPTY DEQUE IS UB
/// AND I WOULD RATHER IT RETURN AN EMPTY ENTRY
/// WHEN ASSUMEDLY GETTING THE FIRST ENTRY (SOMETIMES)
///
/// THIS IS SOLELY TO ADDRESS WHEN THE BANK IS EMPTY
/// </summary>
ShowPlayerPopulation::token ShowPlayerPopulation::get_first_entry() {
        if (bank.empty()) {
                return token {};
        }

        return bank.front();
}

/// <summary>
/// WHY? BECAUSE .back() ON AN EMPTY DEQUE IS UB
/// AND I WOULD RATHER IT RETURN AN EMPTY ENTRY
/// WHEN ASSUMEDLY GETTING THE LAST ENTRY
///
/// THIS IS SOLELY TO ADDRESS WHEN THE BANK IS EMPTY
/// </summary>
ShowPlayerPopulation::token ShowPlayerPopulation::get_last_entry() {
        if (bank.empty()) {
                return token {};
        }

        return bank.back();
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
        std::chrono::sys_time<std::chrono::system_clock::duration> tmpd;
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
                        // set when on the main menu
                        in_main_menu = true;
                } else if (menu_name == "PlayMenuV4Movie") {
                        // set after hitting [Play] on the main menu
                        in_playlist_menu = true;
                } else if (menu_name == "MidGameMenuMovie") {
                        // set when opening up the menu during a game
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
        if ((in_main_menu && show_in_main_menu) || (in_game_menu && show_in_game_menu)
            || (in_playlist_menu && show_in_playlist_menu)) {
                // SHOW THE DAMN NUMBERS, JIM!
                ImGui::SetNextWindowSize(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse;
                if (lock_overlay) {
                        flags |= ImGuiWindowFlags_NoInputs;
                }
                ImGui::Begin("Hey, cutie", NULL, flags);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
                ImGui::SetWindowFontScale(1.3f);
                // ImGui::PushFont(overlay_font_22);
                center_imgui_text(std::vformat(
                                          "POPULATIONS! LAST UPDATED: {0:%r} {0:%D}",
                                          std::make_format_args(get_last_entry().zt))
                                          .c_str());
                ImGui::NewLine();
                ImGui::Indent(20.0f);
                if (ImGui::GetWindowWidth() <= (ImGui::GetIO().DisplaySize.x / 2.0f)) {
                        // less than or equal to half of the width of the screen
                        // = "vertical layout"
                        ImGui::BeginColumns("populationnums_vert", 2, ImGuiColumnsFlags_NoResize);
                        ImGui::TextUnformatted("Total Players Online:");
                        add_underline(col_black);
                        ImGui::NextColumn();
                        center_imgui_text(std::to_string(get_last_entry().total_pop));
                        ImGui::NextColumn();
                        ImGui::TextUnformatted("Total Population in a Game:");
                        add_underline(col_black);
                        ImGui::NextColumn();
                        center_imgui_text(std::to_string(TOTAL_IN_GAME_POP));
                        ImGui::NextColumn();

                        std::vector<std::pair<PlaylistId, int>> playlist_pops;

                        for (const auto & str : SHOWN_PLAYLIST_POPS) {
                                auto pop = population_data[str];
                                for (const auto & popv : pop) {
                                        playlist_pops.push_back({popv.first, popv.second});
                                }
                        }

                        // ImGui::PushFont(overlay_font_18);
                        for (const std::pair<PlaylistId, int> & ppops : playlist_pops) {
                                std::string playliststr = bmhelper::playlist_ids_str_spaced[ppops.first];
                                int         pop         = ppops.second;

                                ImGui::TextUnformatted(std::vformat("{}:", std::make_format_args(playliststr)).c_str());
                                ImGui::NextColumn();
                                center_imgui_text(std::vformat("{}", std::make_format_args(pop)));
                                ImGui::NextColumn();
                        }
                        // ImGui::PopFont();
                        ImGui::EndColumns();
                } else {
                        // greater than half of the width of the screen
                        // "horizontal layout"

                        ImGui::BeginColumns(
                                "pop_horiz_tot",
                                6,
                                ImGuiColumnsFlags_NoResize | ImGuiColumnsFlags_NoBorder);
                        ImGui::TextUnformatted("Total Players Online:");
                        add_underline(col_black);
                        ImGui::NextColumn();
                        center_imgui_text(std::to_string(get_last_entry().total_pop));
                        ImGui::NextColumn();
                        ImGui::TextUnformatted("Total Population in a Game:");
                        add_underline(col_black);
                        ImGui::NextColumn();
                        center_imgui_text(std::to_string(TOTAL_IN_GAME_POP));
                        ImGui::NextColumn();
                        ImGui::EndColumns();

                        size_t mxlines = 0;
                        for (const auto & x : population_data) {
                                mxlines = std::max(mxlines, x.second.size());
                        }
                        // ImGui::PushFont(overlay_font_18);
                        ImGui::BeginColumns("populationnums_horiz", 12, ImGuiColumnsFlags_NoResize);
                        for (int line = 0; line < mxlines; ++line) {
                                for (const auto & playstr : SHOWN_PLAYLIST_POPS) {
                                        if (line >= population_data[playstr].size()) {
                                                // nothing to show :(
                                                ImGui::NextColumn();
                                                ImGui::NextColumn();
                                                continue;
                                        } else {
                                                PlaylistId id  = population_data[playstr][line].first;
                                                int        pop = population_data[playstr][line].second;
                                                ImGui::TextUnformatted(
                                                        std::vformat(
                                                                "{}:",
                                                                std::make_format_args(
                                                                        bmhelper::playlist_ids_str_spaced[id]))
                                                                .c_str());
                                                ImGui::NextColumn();
                                                // MANUAL CENTERING? WTF ... the width isnt being correctly set.
                                                std::string str = std::vformat("{}", std::make_format_args(pop));
                                                ImGui::SetCursorPosX(
                                                        ImGui::GetCursorPosX()
                                                        + ((ImGui::GetContentRegionAvailWidth()
                                                            - ImGui::CalcTextSize(str.c_str()).x)
                                                           * 0.5));
                                                ImGui::TextUnformatted(str.c_str());
                                                ImGui::NextColumn();
                                        }
                                }
                        }
                        // ImGui::PopFont();
                        ImGui::EndColumns();
                }
                // ImGui::PopFont();
                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::End();
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