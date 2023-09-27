#include "client.h"

#include <xmlrpc-c/base.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

Client::Client(const char* endpoint_url) : m_url(endpoint_url) {
    // Responses get really big
    xmlrpc_limit_set(XMLRPC_XML_SIZE_LIMIT_ID, 1024 * 1024 * 64);  // 64 MiB
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
                .name = func_name,
                .addr = func_addr,
                .type = SymbolType::Function,
                .size = func_size,
            };
            symbols.push_back(s);
        }
        return symbols;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to query symbols: ") + e.what());
    }
}
