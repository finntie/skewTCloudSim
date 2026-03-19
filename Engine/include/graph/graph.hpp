#pragma once

#include <vector>

namespace bee::graph
{

/// <summary>
/// Represents a single directed and weighted edge in a graph.
/// </summary>
struct Edge
{
    /// <summary>
    /// The ID of the vertex to which this edge leads.
    /// </summary>
    int m_targetVertex;

    /// <summary>
    /// The traversal cost of the edge, used for path planning.
    /// </summary>
    float m_cost;

    Edge(int targetVertex, float cost) : m_targetVertex(targetVertex), m_cost(cost) {}
};

/// <summary>
/// Represents a graph with directed weighted edges.
/// </summary>
template <typename V>
struct Graph
{
protected:
    std::vector<V> m_vertices;
    std::vector<std::vector<Edge>> m_edges;

public:
    /// <summary>
    /// Gets a vertex with a given ID from the graph.
    /// </summary>
    /// <param name="index">A vertex ID.</param>
    /// <returns>A non-mutable reference to the vertex with the given ID.</returns>
    const V& GetVertex(int index) const { return m_vertices[index]; }

    /// <summary>
    /// Adds a new vertex to the graph.
    /// </summary>
    /// <param name="vertex">A vertex object.</param>
    void AddVertex(const V& vertex)
    {
        m_vertices.push_back(vertex);
        m_edges.push_back({});
    }

    /// <summary>
    /// Adds an edge to the graph.
    /// </summary>
    /// <param name="vertex1">The source vertex ID of the edge.</param>
    /// <param name="vertex2">The target vertex ID of the edge.</param>
    /// <param name="cost">The cost of the edge.</param>
    /// <param name="bidirectional">Whether or not to also create an edge (with the same cost) in the opposite
    /// direction.</param>
    void AddEdge(int vertex1, int vertex2, float cost, bool bidirectional)
    {
        m_edges[vertex1].push_back(Edge(vertex2, cost));
        if (bidirectional) m_edges[vertex2].push_back(Edge(vertex1, cost));
    }

    /// <summary>
    /// Gets all outgoing edges from a given vertex.
    /// </summary>
    /// <param name="vertex">The ID of a vertex in the graph.</param>
    /// <returns>A non-mutable reference to the list of WeightedEdge objects stored for the given vertex.</returns>
    const std::vector<Edge>& GetEdgesFromVertex(int vertex) const { return m_edges[vertex]; }

    /// <summary>
    /// Returns the number of vertices in this graph.
    /// </summary>
    size_t GetNumberOfVertices() const { return m_vertices.size(); }
};

};  // namespace bee::graph