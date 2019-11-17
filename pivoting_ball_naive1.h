#pragma once

#include <cgogn/core/cmap/cmap0.h>
#include <cgogn/core/cmap/cmap2.h>
#include <Eigen/Dense>
#include <math.h>

using CMap0 = cgogn::CMap0;
using CMap2 = cgogn::CMap2;
using Vec3 = Eigen::Vector3d;

struct FrontEdge
{
	CMap0::Vertex startVertex;
	CMap0::Vertex endVertex;
	CMap0::Vertex oppositeVertex;
};

class PivotingBallNaive1
{
private:

	float ballRadius;
	CMap0* points; 
	CMap0::VertexAttribute<Vec3>* pointPositions;
	CMap0::VertexAttribute<Vec3>* pointNormals;

	CMap2* surface; 
	CMap2::VertexAttribute<Vec3>* surfacePositions;

	std::list<FrontEdge> front; 

	bool GetBallCenter(CMap0::Vertex vertex0, CMap0::Vertex vertex1, CMap0::Vertex vertex2, Vec3& center, CMap0::Vertex* sequence);

	void AddEdgeToFront(CMap0::Vertex startVertex, CMap0::Vertex endVertex, CMap0::Vertex oppositeVertex);

	void RemoveEdgeFromFront(std::list<FrontEdge>::iterator edge);

	void PushTriangle(CMap0::Vertex vertex0, CMap0::Vertex vertex1, CMap0::Vertex vertex2);

	std::list<FrontEdge>::iterator FindEdgeOnFront(CMap0::Vertex vertex0, CMap0::Vertex vertex1);

	void JoinOrGlueEdge(CMap0::Vertex vertex0, CMap0::Vertex vertex1, CMap0::Vertex oppositeVertexId);

	bool PivotingBallNaive1::IsEmpty(CMap0::Vertex vertex0, CMap0::Vertex vertex1, CMap0::Vertex vertex2, Vec3 ballCenter);

	std::vector<CMap0::Vertex> GetNeighbors(Vec3 position, float radius);

	bool FindSeed();

	void FinishFront();

public:
	void getSurface
	(
		CMap0& pointSet,
		CMap0::VertexAttribute<Vec3>& pointSetPositions,
		CMap0::VertexAttribute<Vec3>& pointNormals,
		CMap2& surface,
		CMap2::VertexAttribute<Vec3>& surfacePositions,
		float ballRadius
	);
};