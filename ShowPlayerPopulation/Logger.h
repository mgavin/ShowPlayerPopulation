#pragma once
#include <format>
#include "bakkesmod/plugin/bakkesmodplugin.h"

// TODO: create a class out of this someday

extern std::shared_ptr<CVarManagerWrapper> _globalCVarManager;
template <typename... Args> inline void    LOG(const std::string & format_str, Args &&... args) {
      _globalCVarManager->log(std::vformat(format_str, std::make_format_args(args...)));
}

template <typename... Args> inline void LOG(const std::wstring & wformat_str, Args &&... args) {
      _globalCVarManager->log(std::vformat(wformat_str, std::make_wformat_args(args...)));
}
