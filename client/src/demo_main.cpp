#include <iostream>

#include "client.h"

int main(int argc, char** argv) {
    Client c("http://localhost:3662/RPC2/");
    c.logVerbosely();
    auto decomp = c.queryDecompiledFunction(0x1234);
}