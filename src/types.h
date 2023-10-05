#pragma once

//! Small helper library for creating types for x64dbg plugins.

/* clang-format off */
#include <windows.h>

#include <cstdint>
#include <string>
#include <unordered_map>

#include "client.h"
/* clang-format on */

/// Try to add a single type, without checking whether it's dependency types already exist first.
bool addType(Type t);
/// Add all types in dependency-resolved order.
bool addTypes(std::unordered_map<std::string, Type> types);