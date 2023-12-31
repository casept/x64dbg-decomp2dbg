/* clang-format off */

// If these are included after plugin SDK, they cause mysterious compiler errors
#include <exception>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_set>

// Same for these headers, as they include them transitively
#include "client.h"
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "plugin.h"
#include "modules.h"
#include "types.h"

#include <pluginsdk/_plugins.h>
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/dbghelp/dbghelp.h>
#include <pluginsdk/_scriptapi_module.h>

/* clang-format on */

/// Struct storing global knowledge of the plugin.
struct Ctx {
    std::mutex l;        // Lock, as callbacks are concurrent
    std::string apiUrl;  // URL of the decompiler XMLRPC server
    Module modInfo;      //  Info about the module we care about
    bool ready;          // Whether we have all the info needed to start working
    // Addresses we have already decompiled in this session.
    // Until we find a smarter way to invalidate caches, this should prevent
    // the worst of redundant work, at the cost of potentially stale source being shown.
    std::unordered_set<duint> addrsDecompiled;
};

Ctx CTX;

/* String utils which should be part of the goddamn stdlib */
static std::string removeExtension(const std::string &filename) {
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

static bool hasEnding(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

/* Functions for decorating x64dbg output */

static void addSymbol(Symbol s, std::size_t base) {
    std::size_t start = base + s.addr;
    std::size_t end = base + s.addr + s.size;

    switch (s.type) {
        case SymbolType::Function: {
            // Clear any previous auto function here from previous runs
            DbgClearAutoFunctionRange(start, end);
            if (!DbgSetAutoFunctionAt(start, end)) {
                dputs(fmt::format("Failed to add function {} at {:016x}-{:016x}!", s.name, start, end).c_str());
            }

            DbgClearAutoLabelRange(start, end);
            if (!DbgSetAutoLabelAt(start, s.name.c_str())) {
                dputs(
                    fmt::format("Failed to set function name for {} at {:016x}-{:016x}!", s.name, start, end).c_str());
            }
            break;
        }
        default: {
            // TODO: Also use size information
            DbgClearAutoLabelRange(start, end);
            if (!DbgSetAutoLabelAt(start, s.name.c_str())) {
                dputs(fmt::format("Failed to set label/object name for {} at {:016x}!", s.name, start).c_str());
            }
            break;
        }
    }
}

static bool addDecompSourceAsComment(std::size_t base, std::size_t funcOffset, Client &c) {
    // Determine bounds of function
    duint start, end;
    if (!DbgFunctionGet(base + funcOffset, &start, &end)) {
        dputs(fmt::format("Failed to show decompiled function at {:016x}: Failed to get function for address",
                          base + funcOffset)
                  .c_str());
        return false;
    }

    dputs(fmt::format("Getting decomp info for function from {:016x} to {:016x}", start, end).c_str());

    // Check whether any part requires re-decompilation

    for (duint addr = start; addr < end; addr++) {
        // Have we already decompiled this address?
        // If so, it should still be visible, and we should avoid the duplicate work.
        // TODO: Do this at a less granular (i.e. function level).
        const bool alreadyDecomped = CTX.addrsDecompiled.find(addr) != CTX.addrsDecompiled.end();
        if (alreadyDecomped) {
            // Remove any previous decompiler output for function,
            // we'll (re-)decompile it
            DbgClearAutoCommentRange(start, end);
            break;
        }
    }

    // Iterate over all addresses in the function and assign appropriate source to each.
    // FIXME: This is super inefficient, find a better approach to perform this precise mapping.
    std::vector<std::string> lines_seen = {};
    for (duint addr = start; addr < end; addr++) {
        auto decomp = c.queryDecompiledFunction(addr - base);
        CTX.addrsDecompiled.insert(addr);
        if (decomp.line_num != -1) {
            auto line = fmt::format("DECOMP: {}", decomp.source.at(decomp.line_num));
            // Without this, we get multiple comments for the same source line
            if (lines_seen.size() > 0) {
                if (line != lines_seen.back()) {
                    DbgSetAutoCommentAt(addr, line.c_str());
                }
            } else {
                DbgSetAutoCommentAt(addr, line.c_str());
            }
            lines_seen.push_back(line);
        }
    }
    return true;
}

void decompile(duint addr) {
    std::lock_guard<std::mutex>(CTX.l);

    if (!CTX.ready) {
        dputs("Plugin not yet ready to handle decompilation. Ignoring.");
        return;
    }

    // Is this in the module we care about / have decomp on? Check.
    char modName[MAX_MODULE_SIZE];
    if (!DbgGetModuleAt(addr, modName)) {
        dputs("Failed to determine which module address belongs to, aborting!");
        return;
    }

    std::string trimmedName = removeExtension(CTX.modInfo.name);  // Returned module is without ext
    if (std::string(modName) != trimmedName) {
        dputs(fmt::format("Address belongs to module {}, we only care about {}. Ignoring.", modName, trimmedName)
                  .c_str());
        return;
    }

    dputs(
        fmt::format("Fetching decomp for addr {:016x}, base-relative {:016x}", addr, addr - CTX.modInfo.addr).c_str());
    DbgSetAutoCommentAt(addr, "Fetching from decompiler...");
    try {
        Client c(CTX.apiUrl.c_str());
        if (!addDecompSourceAsComment(CTX.modInfo.addr, addr - CTX.modInfo.addr, c)) {
            DbgSetAutoCommentAt(addr, "Decompiler fetch failed, see log!");
        }
    } catch (const std::exception &e) {
        DbgSetAutoCommentAt(addr, "Decompiler fetch failed, see log!");
        throw std::runtime_error(fmt::format("Failed to fetch decompiled source: {}", e.what()));
        return;
    }
}

void decompileRange(duint start, duint end) {
    // FIXME: This is obviously a super inefficient way to do it.
    for (duint addr = start; addr <= end; addr++) {
        decompile(addr);
    }
}

/* Callbacks */

static bool cbConnectCommand(int argc, char **argv) {
    if (argc != 5 || std::string(argv[1]) != "connect") {
        dputs("Usage: " PLUGIN_NAME " connect, host, port");
        return false;
    }

    if (std::string(argv[1]) == "connect") {
        auto host = std::string(argv[2]);

        int portInt = atoi(argv[3]);
        if (portInt < std::numeric_limits<std::uint16_t>::min() ||
            portInt > std::numeric_limits<std::uint16_t>::max()) {
            dputs("port out of range!");
            return false;
        }
        uint16_t port = static_cast<size_t>(portInt);

        // TODO: Connection establishment and symbol application logic
    }
    return true;
}

static void cbPopulateDebugInfo(CBTYPE type, void *cbInfo) {
    (void)type;
    (void)cbInfo;

    const auto g = std::lock_guard<std::mutex>(CTX.l);

    // If we haven't figured out where the module we're targeting lives yet,
    // try doing that (again).
    // TODO: Create config mechanism instead of assuming that main exe is target
    if (!CTX.ready) {
        auto mods = getModules();
        for (const auto mod : mods) {
            if (hasEnding(mod.name, ".exe")) {
                CTX.modInfo = mod;
                CTX.ready = true;
                dputs(fmt::format("Found target module {} at {:016x}", mod.name, mod.addr).c_str());
            }
        }

        Client c(CTX.apiUrl.c_str());
        try {
            c.ping();
        } catch (const std::exception &e) {
            dputs(fmt::format("Failed to ping server: {}", e.what()).c_str());
            return;
        }

        if (CTX.ready) {
            // Query type information
            std::unordered_map<std::string, Type> types{};
            try {
                // Now we're at a point where our debug info won't be lost, populate it
                dputs("Querying structs...");
                auto structs = c.queryStructs();
                // Can't just merge() because it needs to be converted to the type variant first
                for (const auto [name, s] : structs) {
                    types.insert({name, Type(s)});
                }
                // Same for unions
                dputs("Querying unions...");
                auto unions = c.queryUnions();
                for (const auto [name, u] : unions) {
                    types.insert({name, Type(u)});
                }
                // Same for enums
                dputs("Querying enums...");
                auto enums = c.queryEnums();
                for (const auto [name, e] : enums) {
                    types.insert({name, Type(e)});
                }
                // Same for type aliases
                dputs("Querying type aliases...");
                auto aliases = c.queryTypeAliases();
                for (const auto [name, a] : aliases) {
                    types.insert({name, Type(a)});
                }
            } catch (const std::exception &e) {
                dputs(fmt::format("Failed to query types from server: {}", e.what()).c_str());
            }

            // Insert types into x64dbg
            try {
                dputs("Populating types...");
                if (!addTypes(types)) {
                    dputs("Failed to populate types!");
                }
            } catch (const std::exception &e) {
                dputs(fmt::format("Failed to populate types: {}", e.what()).c_str());
            }

            try {
                auto hdrs = c.queryFunctionHeaders();
                dputs("Populating functions...");
                for (const auto hdr : hdrs) {
                    addSymbol(hdr, CTX.modInfo.addr);
                }
            } catch (const std::exception &e) {
                dputs(fmt::format("Failed to query function headers from server: {}", e.what()).c_str());
            }

            try {
                dputs("Populating globals...");
                auto globals = c.queryGlobalVars();
                for (const auto g : globals) {
                    addSymbol(g, CTX.modInfo.addr);
                }
            } catch (const std::exception &e) {
                dputs(fmt::format("Failed to query globals from server: {}", e.what()).c_str());
            }
            dputs("Done");
        }
    }
}

static void cbDecompile(CBTYPE type, void *cbInfo) {
    duint addr;
    if (type == CB_BREAKPOINT) {
        // Can obtain IP from context
        PLUG_CB_BREAKPOINT *bp = reinterpret_cast<PLUG_CB_BREAKPOINT *>(cbInfo);
        if (bp == nullptr) {
            dputs("Breakpoint information structure pointer was null, not executing callback!");
            return;
        }
        addr = bp->breakpoint->addr;
    } else {
        // Have to obtain IP from register dump
        REGDUMP regs;
        if (!DbgGetRegDumpEx(&regs, sizeof(regs))) {
            dputs("Failed to get register dump, aborting!");
            return;
        }

        addr = static_cast<duint>(regs.regcontext.cip);
    }
    try {
        decompile(addr);
    } catch (const std::exception &e) {
        dputs(e.what());
        return;
    }
}

/* GUI functionality */

static void decompileSelection() {
    // There's no callback for the user navigating the disassembly view without
    // setting breakpoints or stepping.
    // In order to nonetheless provide decompilation, we need to
    // track the user's movements in the CPU/disassembly view
    // and decompile each time a new function is entered.

    // Trivial check to ensure we don't do massive amounts of work if nothing changed
    // (i.e. the GUI event causing us to be called was not caused by a disasm selection)
    static duint lastStart, lastEnd;

    SELECTIONDATA s;
    GuiSelectionGet(GUI_DISASSEMBLY, &s);
    if (s.start == lastStart && s.end == lastEnd) {
        return;
    }
    try {
        decompileRange(s.start, s.end);
    } catch (const std::exception &e) {
        dputs(fmt::format("Failed to decompile selection: {}", e.what()).c_str());
    }

    lastStart = s.start;
    lastEnd = s.end;
    GuiUpdateDisassemblyView();
}

void cbGui(CBTYPE type, void *cbInfo) {
    (void)type;
    auto winEvent = reinterpret_cast<PLUG_CB_WINEVENT *>(cbInfo);
    if (winEvent->message == nullptr) {
        return;
    }

    // Could this window event have been triggered by the user
    // navigating the disassembler view?
    if (winEvent->message->message != WM_LBUTTONDOWN) {
        return;
    }

    // TODO: Check whether disasm view is the currently active view,
    // once I find the API for it

    GuiExecuteOnGuiThread(decompileSelection);
}

/* Mandatory exports */

bool pluginInit(PLUG_INITSTRUCT *initStruct) {
    _plugin_registercommand(pluginHandle, PLUGIN_NAME, cbConnectCommand, true);
    _plugin_registercallback(pluginHandle, CB_CREATEPROCESS, cbPopulateDebugInfo);
    _plugin_registercallback(pluginHandle, CB_LOADDLL, cbPopulateDebugInfo);
    _plugin_registercallback(pluginHandle, CB_PAUSEDEBUG, cbDecompile);
    _plugin_registercallback(pluginHandle, CB_WINEVENT, cbGui);

    CTX.l.lock();
    // TODO: Read this from config
    CTX.apiUrl = "http://localhost:3662/RPC2/";
    CTX.l.unlock();
    return true;
}

void pluginStop() { dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle); }

void pluginSetup() { dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle); }
