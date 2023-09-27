#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client_simple.hpp>

enum class SymbolType { Function, Other };

struct Symbol {
    std::string name;
    std::size_t addr;
    SymbolType type;
    std::size_t size;
};

class Client {
   public:
    Client() = delete;
    Client(const char* endpoint_url);
    // Ping the server to check whether the connection works.
    void ping();
    std::vector<Symbol> queryFunctionHeaders();

   private:
    std::string m_url;
};
