#include "graph.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

void Graph::addNode(std::uint32_t node) { m_nodes[node] = {}; }

void Graph::addEdge(std::uint32_t from, std::uint32_t to) {
    if (m_nodes.find(from) == m_nodes.end())
        throw std::runtime_error("Node " + std::to_string(from) + " does not exist");
    m_nodes[from].push_back(to);
}

std::vector<std::vector<std::uint32_t>> Graph::getAllCycles() {
    std::vector<std::vector<std::uint32_t>> cycles{};
    for (const auto& [node, _] : m_nodes) {
        auto nodeCycles = getCyclesFromNode(node);
        cycles.insert(cycles.end(), nodeCycles.begin(), nodeCycles.end());
    }
    return cycles;
}

std::vector<std::vector<std::uint32_t>> Graph::getCyclesFromNodeHelper(
    std::uint32_t node, std::vector<bool>& visited, std::vector<std::uint32_t>& path,
    std::vector<std::vector<std::uint32_t>>& cycles) {
    visited[node] = true;
    for (const auto& neighbor : m_nodes[node]) {
        if (!visited[neighbor]) {
            path.push_back(neighbor);
            getCyclesFromNodeHelper(neighbor, visited, path, cycles);
            path.pop_back();
        } else {
            auto cycle = path;
            cycle.push_back(neighbor);
            cycles.push_back(cycle);
        }
    }
    visited[node] = false;
    return cycles;
}

std::vector<std::vector<std::uint32_t>> Graph::getCyclesFromNode(std::uint32_t node) {
    std::vector<std::vector<std::uint32_t>> cycles{};
    std::vector<std::uint32_t> path{};
    std::vector<bool> visited(m_nodes.size(), false);
    visited[node] = true;
    path.push_back(node);
    getCyclesFromNodeHelper(node, visited, path, cycles);
    return cycles;
}

std::vector<std::uint32_t> Graph::topologicalSort() {
    std::vector<std::uint32_t> sorted{};
    std::vector<bool> visited(m_nodes.size(), false);
    for (const auto& [node, _] : m_nodes) {
        if (!visited[node]) {
            topologicalSortHelper(node, visited, sorted);
        }
    }
    std::reverse(sorted.begin(), sorted.end());
    return sorted;
}

void Graph::topologicalSortHelper(std::uint32_t node, std::vector<bool>& visited, std::vector<std::uint32_t>& sorted) {
    visited[node] = true;
    for (const auto& neighbor : m_nodes[node]) {
        if (!visited[neighbor]) {
            topologicalSortHelper(neighbor, visited, sorted);
        }
    }
    sorted.push_back(node);
}
