/* clang-format off */
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <regex>

#include <fmt/core.h>

#include "types.h"

#include "client.h"
#include "graph.h"

#include "pluginmain.h"

#include <pluginsdk/_plugins.h>
#include <pluginsdk/bridgemain.h>
#include <pluginsdk/dbghelp/dbghelp.h>
/* clang-format on */

/// Mapping of Ghidra type name to x64dbg type name for translation.
/// Types that map to "" have a size that can't be trivially mapped to a x64dbg type and will not be handled right now.
static const std::unordered_map<std::string, std::string> BaseTypes{
    // Ghidra's undefined types
    {"undefined", "Uint8"},
    {"undefined1", "Uint8"},
    {"undefined2", "Uint16"},
    {"undefined3", ""},
    {"undefined4", "Uint32"},
    {"undefined5", ""},
    {"undefined6", ""},
    {"undefined7", ""},
    {"undefined8", "Uint64"},
    // Ghidra's fixed size numeric types
    {"byte", "Uint8"},
    {"sbyte", "Int8"},
    {"word", "Uint16"},
    {"sword", "Int16"},
    {"uint3", ""},
    {"int3", ""},
    {"dword", "Uint32"},
    {"sdword", "Int32"},
    {"uint5", ""},
    {"int5", ""},
    {"uint6", ""},
    {"int6", ""},
    {"uint7", ""},
    {"int7", ""},
    {"qword", "Uint64"},
    {"sqword", "Int64"},
    // Ghidra's misc. fixed-size types
    {"void", "Void"},
    {"wchar16", "Uint16"},
    {"wchar32", "Uint32"},
    // Ghidra's numeric dynamically-sized types
    // These are always fixed-size on Windows.
    {"char", "Int8"},
    {"schar", "Int8"},
    {"uchar", "Uint8"},
    {"short", "Int16"},
    {"ushort", "Uint16"},
    {"int", "Int32"},
    {"uint", "Uint32"},
    {"long", "Int32"},
    {"ulong", "Uint32"},
    {"longlong", "Int64"},
    {"ulonglong", "Uint64"},
    // Ghidra's misc. dynamically-sized types.
    {"wchar_t", "PtrWString"},
    {"pointer", "Pointer"},
    {"float", "Float"},
    {"double", "Double"},
    // Ghidra's string types
    {"String", "PtrString"},
    {"String-utf8", "PtrString"},
    {"TerminatedCString", "PtrString"},
    {"Unicode", "PtrWString"},
    {"TerminatedUnicode", "PtrWString"},
    // TODO: Better mapping
    {"PascalString255", "PtrString"},
    {"PascalString", "PtrString"},
    {"PascalUnicode", "PtrWString"},

};

/// Convert a Ghidra elemental type name to the respective x64dbg type.
/// TODO: Base types should really be translated to generic ones server-side, as the names
/// TODO: are really a decompiler implementation detail.
static std::optional<std::string> translateBaseType(const std::string& type) {
    auto it = BaseTypes.find(type);
    if (it != BaseTypes.end()) {
        return {};
    }
    return {it->second};
}

static std::tuple<std::string, int, bool> parseType(const std::string& declaration) {
    size_t arrayStartPos = declaration.find_last_of("[");
    size_t arrayEndPos = declaration.find_last_of("]");
    bool isPointer = declaration.find('*') != std::string::npos;
    int arraySize = 0;

    if (arrayStartPos != std::string::npos && arrayEndPos != std::string::npos && arrayStartPos < arrayEndPos) {
        std::string arraySizeString = declaration.substr(arrayStartPos + 1, arrayEndPos - arrayStartPos - 1);
        if (arraySizeString.find_first_not_of("0123456789") == std::string::npos) {
            arraySize = std::stoi(arraySizeString);
        }
    }

    size_t pointerPos = declaration.rfind('*');
    if (pointerPos != std::string::npos && (arrayStartPos == std::string::npos || pointerPos > arrayEndPos)) {
        // If there's a '*' after the last ']' or if there are no '[' and ']', we're dealing with a pointer.
        isPointer = true;
    }

    std::string baseTypeName = declaration.substr(0, arrayStartPos);
    baseTypeName.erase(baseTypeName.find_last_not_of("*") + 1);
    return std::make_tuple(baseTypeName, arraySize, isPointer);
}

/// Sort types by their dependencies.
/// Needed because x64dbg requires types to be defined before they are used.
static std::vector<Type> sortTypes(const std::unordered_map<std::string, Type>& types) {
    // Create name->ID mapping for more efficient graph operations on integer IDs
    dputs("Creating type ID mapping...");
    std::unordered_map<std::uint32_t, std::string> iDToName{};
    std::unordered_map<std::string, std::uint32_t> nameToID{};
    std::uint32_t id = 0;
    for (const auto& [name, _] : types) {
        iDToName[id] = name;
        nameToID[name] = id;
        id++;
    }

    // Populate dependency graph
    dputs("Creating type dependency graph...");
    Graph g{};
    for (const auto& [dependent, type] : types) {
        auto dependentID = nameToID[dependent];
        g.addNode(dependentID);
        if (std::holds_alternative<Structure>(type)) {
            for (const auto& member : std::get<Structure>(type).members) {
                auto [baseType, arrayLen, _] = parseType(member.type);
                if (types.find(baseType) != types.end()) {
                    auto baseTypeID = nameToID[baseType];
                    g.addEdge(dependentID, baseTypeID);
                } else {
                    throw std::runtime_error(fmt::format("Unknown type: {}", baseType));
                }
            }
        } else if (std::holds_alternative<Union>(type)) {
            for (const auto& member : std::get<Union>(type).members) {
                auto [baseType, arrayLen, _] = parseType(member.type);
                if (types.find(baseType) != types.end()) {
                    auto baseTypeID = nameToID[baseType];
                    g.addEdge(dependentID, baseTypeID);
                } else {
                    throw std::runtime_error(fmt::format("Unknown type: {}", baseType));
                }
            }
        } else if (std::holds_alternative<TypeAlias>(type)) {
            auto alias = std::get<TypeAlias>(type);
            auto [baseType, _, __] = parseType(alias.type);
            auto baseTypeID = nameToID[baseType];
            g.addEdge(dependentID, baseTypeID);
        } else if (std::holds_alternative<Enum>(type)) {
            // Hardcode to Int32 for now
            auto baseTypeID = nameToID["Int32"];
            g.addEdge(dependentID, baseTypeID);
        } else {
            throw std::runtime_error("Unknown kind of type");
        }
    }

    // Validate acyclicity. For any cycles found, output the cycle to the log and abort.
    dputs("Validating type dependency graph...");
    auto cycles = g.getAllCycles();
    if (!cycles.empty()) {
        for (const auto& cycle : cycles) {
            std::string cycleStr{};
            for (const auto& v : cycle) {
                cycleStr += iDToName[v] + " -> ";
            }
            cycleStr += iDToName[cycle[0]];
            dputs(fmt::format("Cycle detected: {}", cycleStr).c_str());
        }
        throw std::runtime_error("Cycles detected in type dependencies");
    }

    // Now run topological sort
    dputs("Sorting types by dependencies...");
    auto sortedIDs = g.topologicalSort();

    // Convert sorted IDs back to types
    std::vector<Type> sortedTypes{};
    for (const auto& id : sortedIDs) {
        sortedTypes.push_back(types.at(iDToName[id]));
    }
    return sortedTypes;
}

static bool addStructure(Structure s) {
    // There don't seem to be builtin Dbg* functions to manipulate the type system,
    // we have to use the scripting command API
    std::vector<std::string> cmds{};
    cmds.push_back(fmt::format("AddStruct {}", s.name));
    for (const auto m : s.members) {
        auto [baseType, arrayLen, isPointer] = parseType(m.type);
        if (isPointer) {
            cmds.push_back(fmt::format("AppendMember {} {} {}", "Pointer", m.name, arrayLen));
        } else {
            cmds.push_back(fmt::format("AppendMember {} {} {}", baseType, m.name, arrayLen));
        }
    }
    for (const auto& cmd : cmds) {
        dputs(cmd.c_str());
        if (!DbgCmdExecDirect(cmd.c_str())) {
            return false;
        }
    }
    return true;
}

static bool addUnion(Union u) {
    // There don't seem to be builtin Dbg* functions to manipulate the type system,
    // we have to use the scripting command API
    std::vector<std::string> cmds{};
    cmds.push_back(fmt::format("AddUnion {}", u.name));
    for (const auto m : u.members) {
        auto [baseType, arrayLen, isPointer] = parseType(m.type);
        if (isPointer) {
            cmds.push_back(fmt::format("AppendMember {} {} {}", "Pointer", m.name, arrayLen));
        } else {
            cmds.push_back(fmt::format("AppendMember {} {} {}", baseType, m.name, arrayLen));
        }
    }
    for (const auto& cmd : cmds) {
        dputs(cmd.c_str());
        if (!DbgCmdExecDirect(cmd.c_str())) {
            return false;
        }
    }
    return true;
}

static bool addTypeAlias(TypeAlias a) { return DbgCmdExecDirect(fmt::format("AddType {} {}", a.type, a.name).c_str()); }

static bool addEnum(Enum e) {
    // I don't (yet) understand how/whether the x64dbg type system supports enums.
    // For now, we just add them as a type alias for int.
    return addTypeAlias({e.name, "Int32"});
}

bool addType(Type t) {
    if (std::holds_alternative<Structure>(t)) {
        return addStructure(std::get<Structure>(t));
    } else if (std::holds_alternative<Union>(t)) {
        return addUnion(std::get<Union>(t));
    } else if (std::holds_alternative<Enum>(t)) {
        return addEnum(std::get<Enum>(t));
    } else if (std::holds_alternative<TypeAlias>(t)) {
        return addTypeAlias(std::get<TypeAlias>(t));
    } else {
        throw std::runtime_error("Unknown kind of type");
    }
    return true;
}

bool addTypes(std::unordered_map<std::string, Type> types) {
    // Merge in base type aliases
    for (const auto& [name, type] : BaseTypes) {
        types[name] = {TypeAlias{name, type}};
    }

    // Sort types by dependencies
    auto sortedTypes = sortTypes(types);
    // Debug print to verify sort
    for (const auto& t : sortedTypes) {
        dputs(std::visit([](const auto& t) { return t.name; }, t).c_str());
    }

    // Add types
    for (const auto& type : sortedTypes) {
        if (!addType(type)) {
            return false;
        }
    }
    return true;
}