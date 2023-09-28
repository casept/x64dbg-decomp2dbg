/* clang-format off */
// Same header ordering issue as in plugin.cpp

#include "modules.h"
#include <vector>
#include <string>
#include <stdexcept>
#include <fmt/core.h>

#include "pluginmain.h"

#include <pluginsdk/_plugins.h>
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/dbghelp/dbghelp.h>
/* clang-format on */

static bool hasEnding(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

std::vector<Module> getModules() {
    MEMMAP mm;
    if (!DbgMemMap(&mm)) {
        throw std::runtime_error("Failed to get memory map");
    }

    std::vector<Module> modules;
    for (int i = 0; i < mm.count; i++) {
        Module m;
        MEMPAGE mp = mm.page[i];
        m.name = std::string(mp.info);
        // Sometimes, the name is empty
        if (m.name == "") {
            continue;
        }
        // If we get a section or a magic memory area like KUSER_SHARED_DATA,
        // we don't care about them and the below operations will fail on them.
        // Ignore them.
        if (!hasEnding(m.name, ".exe") && !hasEnding(m.name, ".dll") && !hasEnding(m.name, ".drv")) {
            continue;
        }

        // Get the base of this entry
        PVOID base = mp.mbi.BaseAddress;

        // Name is sometimes a full path, but other APIs expect only the filename. Trim.
        m.name = m.name.substr(m.name.find_last_of("/\\") + 1);

        m.addr = DbgModBaseFromName(m.name.c_str());
        if (m.addr == 0) {
            dputs(fmt::format("Failed to get module base from name for {}", m.name).c_str());
            continue;
        }
        modules.push_back(m);
    }
    return modules;
}

Module findModuleByName(const char *name) {
    auto modules = getModules();
    for (const auto mod : modules) {
        if (mod.name == name) {
            return mod;
        }
    }
    throw std::runtime_error(fmt::format("No module with name {} found", name));
}