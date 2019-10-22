/*******************************************************************************
* CGoGN: Combinatorial and Geometric modeling with Generic N-dimensional Maps  *
* Copyright (C) 2015, IGG Group, ICube, University of Strasbourg, France       *
*                                                                              *
* This library is free software; you can redistribute it and/or modify it      *
* under the terms of the GNU Lesser General Public License as published by the *
* Free Software Foundation; either version 2.1 of the License, or (at your     *
* option) any later version.                                                   *
*                                                                              *
* This library is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License  *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU Lesser General Public License     *
* along with this library; if not, write to the Free Software Foundation,      *
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.           *
*                                                                              *
* Web site: http://cgogn.unistra.fr/                                           *
* Contact information: cgogn@unistra.fr                                        *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QMatrix4x4>
#include <QKeyEvent>

#include <QOGLViewer/qoglviewer.h>
#include <cgogn/core/cmap/cmap0.h>
#include <cgogn/core/cmap/cmap2.h>

#include <cgogn/io/map_import.h>
#include <cgogn/io/map_export.h>

#include <cgogn/geometry/algos/bounding_box.h>
#include <cgogn/geometry/algos/normal.h>

#include <cgogn/rendering/map_render.h>
#include <cgogn/rendering/shaders/shader_simple_color.h>
#include <cgogn/rendering/shaders/shader_flat.h>
#include <cgogn/rendering/shaders/shader_vector_per_vertex.h>
#include <cgogn/rendering/shaders/vbo.h>
#include <cgogn/rendering/shaders/shader_bold_line.h>
#include <cgogn/rendering/shaders/shader_point_sprite.h>

#include <cgogn/rendering/shaders/shader_round_point.h>

#include <cgogn/geometry/algos/ear_triangulation.h>

#include <cgogn/rendering/drawer.h>

using namespace cgogn::numerics;

using CMap0 = cgogn::CMap0;
using CMap2 = cgogn::CMap2;

using Vec3 = Eigen::Vector3d;

class Viewer : public QOGLViewer
{
public:

	Viewer();
	CGOGN_NOT_COPYABLE_NOR_MOVABLE(Viewer);

	virtual void draw();
	virtual void init();

	virtual void keyPressEvent(QKeyEvent *);
	void import(const std::string& surface_mesh);
	virtual ~Viewer();
	virtual void closeEvent(QCloseEvent *e);

	void update_surface();

private:

	//2d
	CMap2 cmap2_;
	CMap2::VertexAttribute<Vec3> vertex_position_2_;
	CMap2::EdgeAttribute<uint32> edge_indices_;

	std::unique_ptr<cgogn::rendering::MapRender> render_2_ {nullptr};
	std::unique_ptr<cgogn::rendering::VBO> vbo_pos_2_ {nullptr};
	std::unique_ptr<cgogn::rendering::VBO> vbo_norm_2_ {nullptr};

	std::unique_ptr<cgogn::rendering::ShaderBoldLine::Param> param_edge_ {nullptr};
	std::unique_ptr<cgogn::rendering::ShaderFlat::Param> param_flat_ {nullptr};
	std::unique_ptr<cgogn::rendering::ShaderPointSprite::Param> param_point_sprite_ {nullptr};

	//0d
	CMap0 cmap0_;
	CMap0::VertexAttribute<Vec3> vertex_position_0_;

	std::unique_ptr<cgogn::rendering::MapRender> render_0_ {nullptr};
	std::unique_ptr<cgogn::rendering::VBO> vbo_pos_0_ {nullptr};

	std::unique_ptr<cgogn::rendering::ShaderPointSprite::Param> param_point_sprite_0_ {nullptr};

	//BB
	cgogn::geometry::AABB<Vec3> bb_;
	std::unique_ptr<cgogn::rendering::DisplayListDrawer> drawer_ {nullptr};
	std::unique_ptr<cgogn::rendering::DisplayListDrawer::Renderer> drawer_rend_ {nullptr};

	bool point_set_rendering_ {true};
	bool flat_rendering_ {false};
	bool vertices_rendering_ {false};
	bool edge_rendering_ {false};
	bool bb_rendering_ {true};
};


//
// IMPLEMENTATION
//

std::vector<Vec3> pointsPosition;
std::vector<bool> pointsUsed;
std::vector<Vec3> triangles;
std::vector<uint32_t> front;

void push_to_front(uint32_t edgeStart, uint32_t edgeEnd, uint32_t edgeDirection)
{
	front.push_back(edgeStart);
	front.push_back(edgeEnd);
	front.push_back(edgeDirection);
}

void push_triangle(uint32_t point0, uint32_t point1, uint32_t point2, bool pushEdge0, bool pushEdge1, bool pushEdge2)
{
	Vec3 position0 = pointsPosition[point0];
	Vec3 position1 = pointsPosition[point1];
	Vec3 position2 = pointsPosition[point2];
	float red = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
	float green = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
	float blue = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
	triangles.push_back(position0);
	triangles.push_back(position1);
	triangles.push_back(position2);
	triangles.push_back(Vec3(red, green, blue));

	if (!pointsUsed[point0] || !pointsUsed[point1])
		push_to_front(point0, point1, point2);
	if (!pointsUsed[point1] || !pointsUsed[point2])
		push_to_front(point1, point2, point0);
	if (!pointsUsed[point0] || !pointsUsed[point2])
		push_to_front(point2, point0, point1);

	pointsUsed[point0] = true;
	pointsUsed[point1] = true;
	pointsUsed[point2] = true;
}

double edgePointDistance(Vec3 edgeStart, Vec3 edgeEnd, Vec3 point)
{
	Vec3 start_end = edgeEnd - edgeStart;
	Vec3 start_point = point - edgeStart;
	Vec3 end_point = point - edgeEnd;

	if (start_point.dot(start_end) <= 0.0)
		return start_point.norm();

	if (end_point.dot(start_end) >= 0.0)
		return end_point.norm();

	return start_end.cross(start_point).norm() / start_end.norm();
}

Vec3 getEdgeNormal(Vec3 edgeStart, Vec3 edgeEnd, Vec3 otherPoint)
{
	return ((edgeStart - otherPoint).normalized() + (edgeEnd - otherPoint).normalized()).normalized();
}

void find_seed_triangle()
{
	push_triangle(0, 1, 2, true,true,true);
}

void finish_front()
{
	while (front.size() != 0)
	{
		uint32_t edgeDirection = front.back();
		front.pop_back();
		uint32_t edgeEnd = front.back();
		front.pop_back();
		uint32_t edgeStart = front.back();
		front.pop_back();

		Vec3 edgeStartPosition = pointsPosition[edgeStart];
		Vec3 edgeEndPosition = pointsPosition[edgeEnd];
		Vec3 thirdPoint = pointsPosition[edgeDirection];
		Vec3 edgeNormal = getEdgeNormal(edgeStartPosition, edgeEndPosition, thirdPoint).normalized();

		uint32_t bestIndex = UINT32_MAX;
		double bestDistance = std::numeric_limits<double>::max();
		for (uint32_t i = 0; i < pointsPosition.size(); i++)
		{
			if (i != edgeDirection && i != edgeStart && i != edgeEnd)
			{
				Vec3 point = pointsPosition[i];
				double distance = edgePointDistance(edgeStartPosition, edgeEndPosition, point);
				if (distance < bestDistance)
				{
					bestIndex = i;
					bestDistance = distance;
				}
			}
		}

		if (bestIndex != UINT32_MAX)
		{
			push_triangle(edgeStart, edgeEnd, bestIndex, false, true, true);
		}
	}
}

void Viewer::import(const std::string& point_set)
{
	cgogn::io::import_point_set<Vec3>(cmap0_, point_set);

	vertex_position_0_ = cmap0_.template get_attribute<Vec3, CMap0::Vertex>("position");
	if (!vertex_position_0_.is_valid())
	{
		cgogn_log_error("Viewer::import") << "Missing attribute position. Aborting.";
		std::exit(EXIT_FAILURE);
	}

	vertex_position_2_ = cmap2_.add_attribute<Vec3, CMap2::Vertex>("position");
	edge_indices_ = cmap2_.add_attribute<uint32, CMap2::Edge>("indices");

	cmap0_.foreach_cell([&](CMap0::Vertex vertex)
	{
		pointsPosition.push_back(vertex_position_0_[vertex]);
		pointsUsed.push_back(false);
	});

	find_seed_triangle();
	finish_front();

	cgogn::geometry::compute_AABB(vertex_position_0_, bb_);
	setSceneRadius(cgogn::geometry::diagonal(bb_).norm()/2.0);
	Vec3 center = cgogn::geometry::center(bb_);
	setSceneCenter(qoglviewer::Vec(center[0], center[1], center[2]));
	showEntireScene();
}

Viewer::~Viewer()
{}

void Viewer::closeEvent(QCloseEvent*)
{
	render_2_.reset();
	vbo_pos_2_.reset();
	render_0_.reset();
	vbo_pos_0_.reset();
	drawer_.reset();
	drawer_rend_.reset();
}

Viewer::Viewer() :
	cmap2_(),
	vertex_position_2_(),
	cmap0_(),
	vertex_position_0_(),
	bb_()
{}

void Viewer::keyPressEvent(QKeyEvent *ev)
{
	switch (ev->key())
	{
		case Qt::Key_F:
			flat_rendering_ = !flat_rendering_;
			break;
		case Qt::Key_E:
			edge_rendering_ = !edge_rendering_;
			break;
		case Qt::Key_V:
			vertices_rendering_ = !vertices_rendering_;
			break;
		case Qt::Key_B:
			bb_rendering_ = !bb_rendering_;
			break;
		case Qt::Key_S:
			point_set_rendering_ = !point_set_rendering_;
			break;
		case Qt::Key_X:
		{
			const cgogn::Orbit vertorb = CMap2::Vertex::ORBIT;
			cgogn::io::export_surface(cmap2_, cgogn::io::ExportOptions::create().filename("output.off").position_attribute(vertorb, "position"));
			break;
		}
		default:
			break;
	}
	// enable QGLViewer keys
	QOGLViewer::keyPressEvent(ev);
	//update drawing
	update();
}

void Viewer::draw()
{
	QMatrix4x4 proj;
	QMatrix4x4 view;
	camera()->getProjectionMatrix(proj);
	camera()->getModelViewMatrix(view);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 2.0f);
	if (flat_rendering_)
	{
		param_flat_->bind(proj,view);
		render_2_->draw(cgogn::rendering::TRIANGLES);
		param_flat_->release();
	}
	glDisable(GL_POLYGON_OFFSET_FILL);

	if (vertices_rendering_)
	{
		param_point_sprite_->bind(proj,view);
		render_2_->draw(cgogn::rendering::POINTS);
		param_point_sprite_->release();
	}

	if (edge_rendering_)
	{
		param_edge_->bind(proj,view);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		render_2_->draw(cgogn::rendering::LINES);
		glDisable(GL_BLEND);
		param_edge_->release();
	}


	if (point_set_rendering_)
	{
		param_point_sprite_0_->bind(proj,view);
		render_0_->draw(cgogn::rendering::POINTS);
		param_point_sprite_0_->release();
	}

	if (bb_rendering_)
		drawer_rend_->draw(proj,view);
}

void Viewer::update_surface()
{
	cgogn::rendering::update_vbo(vertex_position_2_, vbo_pos_2_.get());

	render_2_->init_primitives(cmap2_, cgogn::rendering::POINTS);
	render_2_->init_primitives(cmap2_, cgogn::rendering::LINES);
	render_2_->init_primitives(cmap2_, cgogn::rendering::TRIANGLES, &vertex_position_2_);
}

void Viewer::init()
{
	glClearColor(0.1f,0.1f,0.3f,0.0f);

	//CMap0
	vbo_pos_0_ = cgogn::make_unique<cgogn::rendering::VBO>(3);
	render_0_ = cgogn::make_unique<cgogn::rendering::MapRender>();
	param_point_sprite_0_ = cgogn::rendering::ShaderPointSprite::generate_param();
	param_point_sprite_0_->set_position_vbo(vbo_pos_0_.get());
	param_point_sprite_0_->color_ = QColor(0,255,0);
	param_point_sprite_0_->size_= .1f;

	cgogn::rendering::update_vbo(vertex_position_0_, vbo_pos_0_.get());
	render_0_->init_primitives(cmap0_, cgogn::rendering::POINTS);


	//CMap2
	vbo_pos_2_ = cgogn::make_unique<cgogn::rendering::VBO>(3);
	render_2_ = cgogn::make_unique<cgogn::rendering::MapRender>();

	param_point_sprite_ = cgogn::rendering::ShaderPointSprite::generate_param();
	param_point_sprite_->set_position_vbo(vbo_pos_2_.get());
	param_point_sprite_->color_ = QColor(255,0,0);
	param_point_sprite_->size_= 2.5f;

	param_edge_ = cgogn::rendering::ShaderBoldLine::generate_param();
	param_edge_->set_position_vbo(vbo_pos_2_.get());
	param_edge_->color_ = QColor(255,255,0);
	param_edge_->width_ = 1.5f;

	param_flat_ = cgogn::rendering::ShaderFlat::generate_param();
	param_flat_->set_position_vbo(vbo_pos_2_.get());
	param_flat_->front_color_ = QColor(0,200,0);
	param_flat_->back_color_ = QColor(0,0,200);
	param_flat_->ambiant_color_ = QColor(5,5,5);


	// drawer for simple old-school g1 rendering
	drawer_ = cgogn::make_unique<cgogn::rendering::DisplayListDrawer>();
	drawer_rend_= drawer_->generate_renderer();
	drawer_->new_list();
	drawer_->line_width_aa(2.0);
	drawer_->begin(GL_LINE_LOOP);
		drawer_->color3f(1.0,1.0,1.0);
		drawer_->vertex3f(bb_.min()[0],bb_.min()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.min()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.max()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.min()[0],bb_.max()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.min()[0],bb_.max()[1],bb_.max()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.max()[1],bb_.max()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.min()[1],bb_.max()[2]);
		drawer_->vertex3f(bb_.min()[0],bb_.min()[1],bb_.max()[2]);
	drawer_->end();
	drawer_->begin(GL_LINES);
	drawer_->color3f(1.0,1.0,1.0);
		drawer_->vertex3f(bb_.min()[0],bb_.min()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.min()[0],bb_.max()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.min()[0],bb_.min()[1],bb_.max()[2]);
		drawer_->vertex3f(bb_.min()[0],bb_.max()[1],bb_.max()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.min()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.min()[1],bb_.max()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.max()[1],bb_.min()[2]);
		drawer_->vertex3f(bb_.max()[0],bb_.max()[1],bb_.max()[2]);
	drawer_->end();
	/*
	drawer_->point_size(20.0);
	drawer_->begin(GL_POINTS);
	drawer_->color3f(0.0, 0.0, 1.0);
	for (uint32_t i = 0; i < allPoints.size(); i++)
	{
		auto position = allPoints[i];
		drawer_->vertex3f(position[0], position[1], position[2]);
	}
	drawer_->end();*/
	drawer_->begin(GL_TRIANGLES);
	for (uint32_t i = 0; i < triangles.size() / 4; i++)
	{
		auto color = triangles[i * 4 + 3];
		drawer_->color3f(color[0], color[1], color[2]);
		auto position0 = triangles[i*4];
		drawer_->vertex3f(position0[0], position0[1], position0[2]);
		auto position1 = triangles[i * 4+1];
		drawer_->vertex3f(position1[0], position1[1], position1[2]);
		auto position2 = triangles[i * 4+2];
		drawer_->vertex3f(position2[0], position2[1], position2[2]);
	}
	drawer_->end();
	drawer_->end_list();
}

int main(int argc, char** argv)
{
	std::string surface_mesh;
	if (argc < 2)
	{
		cgogn_log_info("simple_viewer") << "USAGE: " << argv[0] << " [filename]";
		return 1;
	}
	else
		surface_mesh = std::string(argv[1]);

	QApplication application(argc, argv);
	qoglviewer::init_ogl_context();

	// Instantiate the viewer.
	Viewer viewer;
	viewer.setWindowTitle("simple_viewer");
	viewer.import(surface_mesh);
	viewer.show();

	// Run main loop.
	return application.exec();
}
