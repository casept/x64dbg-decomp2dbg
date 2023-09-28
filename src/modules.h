#pragma once

//! Small helper library for enumerating modules in memory for x64dbg plugins.

/* clang-format off */
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>
/* clang-format on */

struct Module {
    std::string name;
    std::size_t addr;
};

std::vector<Module> getModules();
Module findModuleByName(const char* name);