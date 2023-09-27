#include "client.h"

#include <fmt/core.h>
#include <xmlrpc-c/base.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

Client::Client(const char* endpoint_url) : m_verbose(false), m_url(endpoint_url) {
    // Responses get really big
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 1024 * 1024 * 64);  // 64 MiB
}

void Client::logVerbosely() { m_verbose = true; }

void Client::log(const std::string& msg) {
    if (m_verbose) {
        fmt::print("[Client] {}\n", msg);
    }
}

void Client::ping() {
    try {
        xmlrpc_c::clientSimple c{};
        xmlrpc_c::value out;
        c.call(m_url, "d2d.ping", &out);
        bool ok = xmlrpc_c::value_boolean(out.cValue()).cvalue();
        if (!ok) {
            throw std::runtime_error("Server responded with false");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to ping: ") + e.what());
    }
}

std::vector<Symbol> Client::queryFunctionHeaders() {
    try {
        xmlrpc_c::clientSimple c{};
        xmlrpc_c::value out;
        c.call(m_url, "d2d.function_headers", &out);
        std::vector<Symbol> symbols;
        auto top_level = xmlrpc_c::value_struct(out.cValue()).cvalue();
        // This is a map, where the function address is the key
        for (const auto entry : top_level) {
            auto mid_level = xmlrpc_c::value_struct(xmlrpc_c::value(entry.second).cValue()).cvalue();
            // FIXME: This loses precision, convert directly to size_t
            std::size_t func_addr;
            // +2 to ignore the "0x" which from_chars can't parse
            std::from_chars(entry.first.data() + 2, entry.first.data() + entry.first.size(), func_addr, 16);
            std::string func_name = "";
            std::size_t func_size = 0;

            for (const auto entry2 : mid_level) {
                auto name = entry2.first;
                if (name == "name") {
                    func_name =
                        static_cast<std::string>(xmlrpc_c::value_string(xmlrpc_c::value(entry2.second).cValue()));
                } else if (name == "size") {
                    func_size =
                        static_cast<std::size_t>(xmlrpc_c::value_int(xmlrpc_c::value(entry2.second).cValue()).cvalue());
                } else {
                    throw std::runtime_error(std::string("Failed to parse response: Unknown key ") + name);
                }
            }
            Symbol s = {
                .type = SymbolType::Function,
                .name = func_name,
                .addr = func_addr,
                .size = func_size,
            };
            symbols.push_back(s);
        }
        return symbols;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to query symbols: ") + e.what());
    }
}

DecompiledFunction Client::queryDecompiledFunction(std::size_t addr) {
    try {
        log("Querying decompiled functions...");
        xmlrpc_c::clientSimple c{};
        xmlrpc_c::value out;
        c.call(m_url, "d2d.decompile", xmlrpc_c::paramList().add(xmlrpc_c::value_int(addr)), &out);
        log("RPC call OK, processing...");

        DecompiledFunction f{};
        bool have_name = false;
        bool have_source = false;
        bool have_line_num = false;

        auto top_level = xmlrpc_c::value_struct(out.cValue()).cvalue();
        // This is a map, where the function address is the key
        for (const auto entry : top_level) {
            auto key = entry.first;
            if (key == "curr_line") {
                have_line_num = true;
                // Line number
                f.line_num = xmlrpc_c::value_int(xmlrpc_c::value(entry.second).cValue());
                log(fmt::format("Line number for address: {}", f.line_num));
            } else if (key == "decompilation") {
                have_source = true;
                // Decompiled source of the function, line-by-line
                auto val = xmlrpc_c::value(entry.second).cValue();
                for (const auto& decomp : xmlrpc_c::value_array(val).cvalue()) {
                    auto decomp_line = static_cast<std::string>(xmlrpc_c::value_string(xmlrpc_c::value(decomp)));
                    f.source.push_back(decomp_line);
                    log(fmt::format("Source line: {}", decomp_line));
                }
            } else if (key == "func_name") {
                have_name = true;
                // Name of the function
                auto val = xmlrpc_c::value(entry.second);
                f.name = static_cast<std::string>(xmlrpc_c::value_string(val.cValue()));
                log(fmt::format("Name of function: {}", f.name));
            } else {
                throw std::runtime_error("Encountered unexpected key: " + key);
            }
        }

        if (have_name && have_line_num && have_source) {
            log("Retrieving decompiler info complete");
            return f;
        } else {
            throw std::runtime_error(
                fmt::format("Missing required keys: have_name: {}, have_line_num: {}, have_source: {}", have_name,
                            have_line_num, have_source));
        }
    } catch (const std::exception& e) {
        std::string err = std::string("Failed to query function decompilation: ") + e.what();
        throw std::runtime_error(err);
    }
}
