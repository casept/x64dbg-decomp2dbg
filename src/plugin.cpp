/* clang-format off */

// If these are included after plugin SDK, they cause mysterious compiler errors
#include <exception>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

// Same for these headers, as they include them transitively
#include "client.h"

#include "plugin.h"

#include <pluginsdk/_plugins.h>
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/dbghelp/dbghelp.h>

/* clang-format on */

static void addSymbol(Symbol s, std::size_t base) {
    std::string msg = "Adding symbol " + s.name + " at " + std::to_string(s.addr);
    dputs(msg.c_str());
    switch (s.type) {
        case SymbolType::Function: {
            if (!DbgFunctionAdd(base + s.addr, base + s.addr + s.size)) {
                dputs("Failed to add function!");
            }

            if (!DbgSetLabelAt(base + s.addr, s.name.c_str())) {
                dputs("Failed to set function name!");
            }
            break;
        }
        default: {
            // TODO: Also use size information
            if (!DbgSetLabelAt(base + s.addr, s.name.c_str())) {
                dputs("Failed to set label/object name!");
            }
            break;
        }
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
        std::string msg = std::string("Failed to query function headers from server: ") + e.what();
        dputs(msg);
    }
}

// Initialize your plugin data here.
bool pluginInit(PLUG_INITSTRUCT *initStruct) {
    _plugin_registercommand(pluginHandle, PLUGIN_NAME, cbConnectCommand, true);
    // We need to wait until process creation to load symbols as they're ignored otherwise
    _plugin_registercallback(pluginHandle, CB_CREATEPROCESS, cbCreateProcess);
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
