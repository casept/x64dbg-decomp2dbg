#include <iostream>

#include "client.h"

int main(int argc, char** argv) {
    Client c("http://localhost:3662/RPC2/");
    auto hdrs = c.queryFunctionHeaders();
    for (const auto& hdr : hdrs) {
        std::cout << "name: " << hdr.name << ", addr: " << hdr.addr << ", size: " << hdr.size << std::endl;
    }
}