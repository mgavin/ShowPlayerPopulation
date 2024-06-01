#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_sugar.hpp"

namespace imgui_helper {
struct PluginSettings {
        float hcolos[12] = {-1};  // horizontal overlay offsets

        float vcolos[2] = {-1};  // vertical overlay offsets

        bool lock_overlay           = false;
        bool hide_overlay_title_bar = false;
        bool lock_overlay_borders   = false;
        bool show_overlay_borders   = false;

        ImVec4 chosen_overlay_color      = {1.0f, 1.0f, 1.0f, 0.9f};
        ImVec4 chosen_overlay_text_color = {0.0f, 0.0f, 0.0f, 1.0f};
};
}  // namespace imgui_helper

#ifdef IMGUI_SUGAR_CONCAT1  // some kind of conditional for the existence of imsugar?

// imgui_sugar.hpp additional code:
namespace ImGuiSugar {
/// <summary>
/// ImGui helper function that pushes some values that make an item appear "inactive"
/// </summary>
inline void PushItemDisabled() {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

/// <summary>
/// ImGui helper function that pops some values that make an item appear "inactive"
/// </summary>
inline void PopItemDisabled() {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
}
}  // namespace ImGuiSugar

#define IMGUI_SUGAR_PARENT_SCOPED_STYLE_COND_VOID_0(BEGIN, END, COND)                                     \
        const std::unique_ptr<ImGuiSugar::BooleanGuard<true>> IMGUI_SUGAR_CONCAT1(_ui_scope_, __LINE__) = \
                ((COND) ? (std::make_unique<ImGuiSugar::BooleanGuard<true>>(IMGUI_SUGAR_ES(BEGIN), &END)) \
                        : (nullptr));

#define IMGUI_SUGAR_SCOPED_STYLE_COND_VOID_0(BEGIN, END, COND)                                                  \
        if (const std::unique_ptr<ImGuiSugar::BooleanGuard<true>> _ui_scope_guard =                             \
                    ((COND) ? (std::make_unique<ImGuiSugar::BooleanGuard<true>>(IMGUI_SUGAR_ES_0(BEGIN), &END)) \
                            : (nullptr));                                                                       \
            true)

#define maybe_Disabled(flag) \
        IMGUI_SUGAR_SCOPED_STYLE_COND_VOID_0(ImGuiSugar::PushItemDisabled, ImGuiSugar::PopItemDisabled, flag)

#define group_Disabled(flag) \
        IMGUI_SUGAR_PARENT_SCOPED_STYLE_COND_VOID_0(ImGuiSugar::PushItemDisabled, ImGuiSugar::PopItemDisabled, flag)

#endif
