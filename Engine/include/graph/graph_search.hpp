#pragma once

#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "graph/euclidean_graph.hpp"
#include "graph/graph.hpp"

/// <summary>
/// A namespace containing functions related to graph search.
/// </summary>
namespace bee::graph
{
struct VertexWithPosition;

/// A pre-defined A* heuristic function: the Euclidean distance between two vertices.
extern const std::function<float(const VertexWithPosition&, const VertexWithPosition&)> AStarHeuristic_EuclideanDistance;

/// <summary>
/// Tries to compute a path through a graph using the A* search algorithm.
/// </summary>
/// <param name="graph">The graph through which the path should be planned.</param>
/// <param name="start">The index of the start vertex.</param>
/// <param name="goal">The index of the goal vertex.</param>
/// <param name="heuristic">The heuristic function, used for estimating the cost from any vertex to the goal.</param>
/// <returns>A list of vertex indices, representing the path from start to goal. Returns an empty list if no path is
/// found.</returns>
template <typename V>
std::vector<int> AStar(const Graph<V>& graph,
                       const int start,
                       const int goal,
                       std::function<float(const V&, const V&)> heuristic)
{
    // If the start and goal vertex are the same, there's nothing to compute.
    if (start == goal) return {start};

    const V& startVertex = graph.GetVertex(start);
    const V& goalVertex = graph.GetVertex(goal);

    /// <summary>
    /// An item in the A* open list.
    /// </summary>
    struct OpenListItem
    {
        int vertex;
        float g, h;

        OpenListItem(int _vertex, float _g, float _h) : vertex(_vertex), g(_g), h(_h) {}

        /// <summary>
        /// Compares two OpenListItems by their value of g + h.
        /// This is required for them to be storable in a priority queue.
        /// NOTE: we implement it using a > operator instead of <,
        /// otherwise std::priority_queue will give us the highest instead of the lowest item!
        /// </summary>
        bool operator<(const OpenListItem& other) const { return g + h > other.g + other.h; }
    };

    // Open list: items that may be checked in the future, ordered by g+h values.
    std::priority_queue<OpenListItem> openList;

    // Add the first item to the open list
    openList.push(OpenListItem(start, 0, heuristic(startVertex, goalVertex)));

    // Closed list: vertices that do not need to be checked anymore.
    // An alternative implementation could be a "visited" flag per vertex, but this has disadvantages.
    std::unordered_set<int> closedList;

    /// <summary>
    /// Represents a reference to the best known data about a vertex during the A* search.
    /// A single BestVertexData object stores both the preceding vertex ID and the corresponding path cost.
    /// </summary>
    struct BestVertexData
    {
        int previousVertex;
        float pathCost;
        BestVertexData() : previousVertex(-1), pathCost(0) {}
        BestVertexData(int _previousVertex, float _pathCost) : previousVertex(_previousVertex), pathCost(_pathCost) {}
    };
    std::unordered_map<int, BestVertexData> bestVertexData;
    bestVertexData[start] = {-1, 0};

    while (!openList.empty())
    {
        // Get the most promising vertex
        OpenListItem item = openList.top();
        openList.pop();
        int v1 = item.vertex;

        // If this vertex is already in the closed list, ignore it
        if (closedList.find(v1) != closedList.end()) continue;

        // Add the vertex to the closed list now, so we'll never visit it again
        closedList.insert(v1);

        // If this vertex is the goal, then we're finished!
        if (v1 == goal)
        {
            // Trace the path back to the start
            int v = goal;
            std::vector<int> result;
            while (v != -1)
            {
                result.push_back(v);
                v = bestVertexData[v].previousVertex;
            }
            std::reverse(result.begin(), result.end());

            // Return the path
            return result;
        }

        // Check all outgoing edges
        for (const Edge& edge : graph.GetEdgesFromVertex(v1))
        {
            int v2 = edge.m_targetVertex;

            // If v2 is already in the closed list, ignore this edge
            if (closedList.find(v2) != closedList.end()) continue;

            // Compute the path cost via v1 to v2
            float gNew = item.g + edge.m_cost;

            // if this is the best predecessor/path to v2 known so far, store that
            const auto& predecessor = bestVertexData.find(v2);
            if (predecessor == bestVertexData.end() || gNew < predecessor->second.pathCost) bestVertexData[v2] = {v1, gNew};

            // Add the target vertex to the open list, with an increased path cost
            openList.push(OpenListItem(v2, gNew, heuristic(graph.GetVertex(v2), goalVertex)));
        }
    }

    return {};
}
};  // namespace bee::graph
