#include "graph/graph_search.hpp"

#include "glm/glm.hpp"

// Heuristic function: Euclidean distance between two vertices
std::function<float(const bee::graph::VertexWithPosition&, const bee::graph::VertexWithPosition&)> const
    bee::graph::AStarHeuristic_EuclideanDistance =
        [](const bee::graph::VertexWithPosition& v1, const bee::graph::VertexWithPosition& v2)
{ return glm::distance(v1.position, v2.position); };