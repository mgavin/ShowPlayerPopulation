#pragma once
#include <format>
#include "bakkesmod/plugin/bakkesmodplugin.h"

// TODO: create a class out of this someday

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
template<typename S, typename... Args>
void LOG(const S & format_str, Args &&... args) {
        _globalCvarManager->log(std::vformat(format_str, std::make_format_args(args...)));
}
