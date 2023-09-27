#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

enum class SymbolType { Function, Other };

/// Information about a symbol known to the decompiler.
/// Addresses are relative to the module base.
struct Symbol {
    SymbolType type;
    std::string name;
    std::size_t addr;
    std::size_t size;
};

struct DecompiledFunction {
    std::string name;
    /// The decompiled code.
    /// TODO: Finer-grained mapping to asm.
    std::vector<std::string> source;
    /// Not quite sure what exactly this means yet.
    /// Probably which line in the decompiler output corresponds to the address queried.
    int line_num;
};

class Client {
   public:
    Client() = delete;
    Client(const char* endpoint_url);
    /// Ping the server to check whether the connection works.
    void ping();
    /// Enable verbose logging.
    void logVerbosely();
    /// Query basic information about all functions known to the decompiler.
    std::vector<Symbol> queryFunctionHeaders();
    /// Query a detailed decompilation of a function containing the given
    /// module base-relative address.
    DecompiledFunction queryDecompiledFunction(std::size_t addr);

   private:
    void log(const std::string& msg);

    bool m_verbose;
    std::string m_url;
};
