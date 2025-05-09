// -*- mode: c++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, JSK Lab
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the JSK Lab nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include "jsk_recognition_utils/pcl/ear_clipping_patched.h"
#include <pcl/surface/ear_clipping.h>
#include <pcl/conversions.h>
#include <pcl/pcl_config.h>

/////////////////////////////////////////////////////////////////////////////////////////////
bool
pcl::EarClippingPatched::initCompute ()
{
  points_.reset (new pcl::PointCloud<pcl::PointXYZ>);

  if (!MeshProcessing::initCompute ())
    return (false);
  fromPCLPointCloud2 (input_mesh_->cloud, *points_);

  return (true);
}

/////////////////////////////////////////////////////////////////////////////////////////////
void
pcl::EarClippingPatched::performProcessing (PolygonMesh& output)
{
  output.polygons.clear ();
  output.cloud = input_mesh_->cloud;
  for (int i = 0; i < static_cast<int> (input_mesh_->polygons.size ()); ++i)
    triangulate (input_mesh_->polygons[i], output);
}

/////////////////////////////////////////////////////////////////////////////////////////////
void
pcl::EarClippingPatched::triangulate (const Vertices& vertices, PolygonMesh& output)
{
  const size_t n_vertices = vertices.vertices.size ();

  if (n_vertices < 3)
    return;
  else if (n_vertices == 3)
  {
    output.polygons.push_back( vertices );
    return;
  }

  Vertices remaining_vertices = vertices;
  size_t count = triangulateClockwiseVertices(remaining_vertices, output);

  // if the input vertices order is anti-clockwise, it always left a
  // convex polygon and start infinite loops, which means will left more
  // than 3 points.
  if (remaining_vertices.vertices.size() < 3) return;

  output.polygons.erase(output.polygons.end(), output.polygons.end() + count);
  remaining_vertices.vertices.resize(n_vertices);
  for (size_t v = 0; v < n_vertices; v++)
      remaining_vertices.vertices[v] = vertices.vertices[n_vertices - 1 - v];
  triangulateClockwiseVertices(remaining_vertices, output);
}

/////////////////////////////////////////////////////////////////////////////////////////////
size_t
pcl::EarClippingPatched::triangulateClockwiseVertices (Vertices& vertices, PolygonMesh& output)
{
  // triangles count
  size_t count = 0;

  // Avoid closed loops.
  if (vertices.vertices.front () == vertices.vertices.back ())
    vertices.vertices.erase (vertices.vertices.end () - 1);

  // null_iterations avoids infinite loops if the polygon is not simple.
  for (int u = static_cast<int> (vertices.vertices.size ()) - 1, null_iterations = 0;
      vertices.vertices.size () > 2 && null_iterations < static_cast<int >(vertices.vertices.size () * 2);
      ++null_iterations, u = (u+1) % static_cast<int> (vertices.vertices.size ()))
  {
    int v = (u + 1) % static_cast<int> (vertices.vertices.size ());
    int w = (u + 2) % static_cast<int> (vertices.vertices.size ());

    if (vertices.vertices.size() == 3 || isEar (u, v, w, vertices))
    {
      Vertices triangle;
      triangle.vertices.resize (3);
      triangle.vertices[0] = vertices.vertices[u];
      triangle.vertices[1] = vertices.vertices[v];
      triangle.vertices[2] = vertices.vertices[w];
      output.polygons.push_back (triangle);
      vertices.vertices.erase (vertices.vertices.begin () + v);
      null_iterations = 0;
      count++;
    }
  }
  return count;
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool
pcl::EarClippingPatched::isEar (int u, int v, int w, const Vertices& vertices)
{
  Eigen::Vector3f p_u, p_v, p_w;
  p_u = points_->points[vertices.vertices[u]].getVector3fMap();
  p_v = points_->points[vertices.vertices[v]].getVector3fMap();
  p_w = points_->points[vertices.vertices[w]].getVector3fMap();

  const float eps = 1e-15f;
  Eigen::Vector3f p_vu, p_vw;
  p_vu = p_u - p_v;
  p_vw = p_w - p_v;

  // 1: Avoid flat triangles and concave vertex
  Eigen::Vector3f cross = p_vu.cross(p_vw);
  if ((cross[2] > 0) || (cross.norm() < eps))
    return (false);

  Eigen::Vector3f p;
  // 2: Check if any other vertex is inside the triangle.
  for (int k = 0; k < static_cast<int> (vertices.vertices.size ()); k++)
  {
    if ((k == u) || (k == v) || (k == w))
      continue;
    p = points_->points[vertices.vertices[k]].getVector3fMap();

    if (isInsideTriangle (p_u, p_v, p_w, p))
      return (false);
  }

  // 3: Check if the line segment uw lies completely inside the polygon.
  // Here we suppose simple polygon (all edges do not intersect by themselves),
  // so we only check if the middle point of line segment uw is inside the polygon.
  Eigen::Vector3f p_i0, p_i1;
  Eigen::Vector3f p_mid_uw = (p_u + p_w) / 2.0;
  // HACK: avoid double-counting intersection at polygon vertices
  Eigen::Vector3f p_inf = p_mid_uw + (p_v - p_mid_uw) * 1e15f + p_vu * 1e10f;
  int intersect_count = 0;
  for (int i = 0; i < static_cast<int>(vertices.vertices.size()); i++)
  {
    p_i0 = points_->points[vertices.vertices[i]].getVector3fMap();
    p_i1 = points_->points[vertices.vertices[(i + 1) % static_cast<int>(vertices.vertices.size())]].getVector3fMap();
    if (intersect(p_mid_uw, p_inf, p_i0, p_i1))
      intersect_count++;
  }
  if (intersect_count % 2 == 0)
    return (false);

  return (true);
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool
pcl::EarClippingPatched::isInsideTriangle (const Eigen::Vector3f& u,
                                    const Eigen::Vector3f& v,
                                    const Eigen::Vector3f& w,
                                    const Eigen::Vector3f& p)
{
  // see http://www.blackpawn.com/texts/pointinpoly/default.html
  // Barycentric Coordinates
  Eigen::Vector3f v0 = w - u;
  Eigen::Vector3f v1 = v - u;
  Eigen::Vector3f v2 = p - u;

  // Compute dot products
  float dot00 = v0.dot(v0);
  float dot01 = v0.dot(v1);
  float dot02 = v0.dot(v2);
  float dot11 = v1.dot(v1);
  float dot12 = v1.dot(v2);

  // Compute barycentric coordinates
  float invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
  float a = (dot11 * dot02 - dot01 * dot12) * invDenom;
  float b = (dot00 * dot12 - dot01 * dot02) * invDenom;

  // Check if point is in triangle
  return (a >= 0) && (b >= 0) && (a + b < 1);
}

/////////////////////////////////////////////////////////////////////////////////////////////
bool
pcl::EarClippingPatched::intersect (const Eigen::Vector3f& p0,
                                    const Eigen::Vector3f& p1,
                                    const Eigen::Vector3f& p2,
                                    const Eigen::Vector3f& p3)
{
  // See http://mathworld.wolfram.com/Line-LineIntersection.html
  Eigen::Vector3f a = p1 - p0;
  Eigen::Vector3f b = p3 - p2;
  Eigen::Vector3f c = p2 - p0;

  // Parallel line segments do not intersect each other.
  if (a.cross(b).norm() == 0)
    return (false);

  // Compute intersection of two lines.
  float s = (c.cross(b)).dot(a.cross(b)) / ((a.cross(b)).norm() * (a.cross(b)).norm());
  float t = (c.cross(a)).dot(a.cross(b)) / ((a.cross(b)).norm() * (a.cross(b)).norm());

  // Check if the intersection is inside the line segments.
  return ((s >= 0 && s <= 1) && (t >= 0 && t <= 1));
}
