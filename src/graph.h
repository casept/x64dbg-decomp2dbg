#pragma once

//! A minimal implementation of a graph and some operations on it, because boost::Graph
//! is completely inscrutable and makes simple things near-impossible.

#include <cstdint>
#include <unordered_map>
#include <vector>

class Graph {
   public:
    void addNode(std::uint32_t node);
    void addEdge(std::uint32_t node1, std::uint32_t node2);
    std::vector<std::vector<std::uint32_t>> getAllCycles();
    std::vector<std::uint32_t> topologicalSort();

   private:
    std::vector<std::vector<std::uint32_t>> getCyclesFromNode(std::uint32_t node);
    std::vector<std::vector<std::uint32_t>> getCyclesFromNodeHelper(std::uint32_t node, std::vector<bool>& visited,
                                                                    std::vector<std::uint32_t>& path,
                                                                    std::vector<std::vector<std::uint32_t>>& cycles);
    void topologicalSortHelper(std::uint32_t node, std::vector<bool>& visited, std::vector<std::uint32_t>& sorted);

    // Adjacency list representation of the graph.
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> m_nodes;
};