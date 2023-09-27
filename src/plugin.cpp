/* clang-format off */

// If these are included after plugin SDK, they cause mysterious compiler errors
#include <exception>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

// Same for these headers, as they include them transitively
#include "client.h"
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "plugin.h"

#include <pluginsdk/_plugins.h>
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/dbghelp/dbghelp.h>

/* clang-format on */

static void addSymbol(Symbol s, std::size_t base) {
    dputs(fmt::format("Adding symbol {} at {}", s.name, s.addr).c_str());
    std::size_t start = base + s.addr;
    std::size_t end = base + s.addr + s.size;

    switch (s.type) {
        case SymbolType::Function: {
            // Clear any previous auto function here from previous runs
            DbgClearAutoFunctionRange(start, end);
            if (!DbgSetAutoFunctionAt(start, end)) {
                dputs("Failed to add function!");
            }

            DbgClearAutoLabelRange(start, end);
            if (!DbgSetAutoLabelAt(start, s.name.c_str())) {
                dputs("Failed to set function name!");
            }
            break;
        }
        default: {
            // TODO: Also use size information
            DbgClearAutoLabelRange(start, end);
            if (!DbgSetAutoLabelAt(start, s.name.c_str())) {
                dputs("Failed to set label/object name!");
            }
            break;
        }
    }
}

static void addDecompSourceAsComment(std::size_t base, std::size_t funcAddr, const DecompiledFunction &df) {
    // Remove any previous auto comments for function
    DbgClearAutoCommentRange(base + funcAddr, base + funcAddr);

    dputs(fmt::format("Decompiled: line_num: {}, name: {}, source_lines: {}", df.line_num, df.name, df.source.size())
              .c_str());
    auto decompStr = fmt::format("DECOMP: {}", fmt::join(df.source, "\n "));
    if (!DbgSetAutoCommentAt(base + funcAddr, decompStr.c_str())) {
        dputs(fmt::format("Failed to add decompiled source as comment at {:08x}", base + funcAddr).c_str());
    }
}

static bool cbConnectCommand(int argc, char **argv) {
    if (argc != 5 || std::string(argv[1]) != "connect") {
        dputs("Usage: " PLUGIN_NAME " connect, host, port");

        // Return false to indicate failure (used for scripting)
        return false;
    }

    // NOTE: Look at x64dbg-sdk/pluginsdk/bridgemain.h for a list of available
    // functions. The Script:: namespace and DbgFunctions()->... are also good to
    // check out.

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

static void cbCreateProcess(CBTYPE type, void *cbInfo) {
    (void)type;
    (void)cbInfo;

    // Now we're at a point where our debug info won't be lost, populate it
    try {
        Client c("http://localhost:3662/RPC2/");
        c.ping();
        auto hdrs = c.queryFunctionHeaders();
        for (const auto hdr : hdrs) {
            addSymbol(hdr, 0x00400000);
        }
    } catch (const std::exception &e) {
        dputs(fmt::format("Failed to query function headers from server: {}", e.what()).c_str());
        return;
    }
}

static void cbBreakpoint(CBTYPE type, void *cbInfo) {
    (void)type;
    PLUG_CB_BREAKPOINT *bp = reinterpret_cast<PLUG_CB_BREAKPOINT *>(cbInfo);
    if (bp == nullptr) {
        dputs("Breakpoint information structure pointer was null, not executing callback!");
        return;
    }

    const std::size_t base = 0x00400000;
    // FIXME: Only decorate if it's in a module we know of (compare bp->breakpoint->mod or so)
    // FIXME: Integrate some kind of caching, as fetching is slow
    // FIXME: Printing for 64 bit
    dputs(fmt::format("Fetching decomp for BP at addr {:08x}, base-relative {:08x}", bp->breakpoint->addr,
                      bp->breakpoint->addr - base)
              .c_str());
    DbgSetAutoCommentAt(bp->breakpoint->addr, "Fetching from decompiler...");
    try {
        Client c("http://localhost:3662/RPC2/");
        auto decomp = c.queryDecompiledFunction(bp->breakpoint->addr - base);
        addDecompSourceAsComment(base, bp->breakpoint->addr - base, decomp);
    } catch (const std::exception &e) {
        dputs(fmt::format("Failed to fetch decompiled source: {}", e.what()).c_str());
        DbgSetAutoCommentAt(bp->breakpoint->addr, "Decompiler fetch failed, see log!");
        return;
    }
}

// Initialize your plugin data here.
bool pluginInit(PLUG_INITSTRUCT *initStruct) {
    _plugin_registercommand(pluginHandle, PLUGIN_NAME, cbConnectCommand, true);
    // We need to wait until process creation to load symbols as they're ignored otherwise
    _plugin_registercallback(pluginHandle, CB_CREATEPROCESS, cbCreateProcess);
    _plugin_registercallback(pluginHandle, CB_BREAKPOINT, cbBreakpoint);
    return true;
}

// Deinitialize your plugin data here.
// NOTE: you are responsible for gracefully closing your GUI
// This function is not executed on the GUI thread, so you might need
// to use WaitForSingleObject or similar to wait for everything to close.
void pluginStop() {
    // Prefix of the functions to call here: _plugin_unregister

    dprintf("pluginStop(pluginHandle: %d)\n", pluginHandle);
}

// Do GUI/Menu related things here.
// This code runs on the GUI thread: GetCurrentThreadId() ==
// GuiGetMainThreadId() You can get the HWND using GuiGetWindowHandle()
void pluginSetup() {
    // Prefix of the functions to call here: _plugin_menu

    dprintf("pluginSetup(pluginHandle: %d)\n", pluginHandle);
}
