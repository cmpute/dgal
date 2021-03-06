// Copyright (c) 2020 Jacob Zhong
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 

/* 
 * This file contains implementations of algorithms of simple geometries suitable for using in GPU.
 * One can turn to shapely, boost.geometry or CGAL for a complete functionality of geometry operations.
 * 
 * Predicates:
 *      .contains: whether the shape contains another shape
 *      .intersects: whether the shape has intersection with another shaoe
 * 
 * Unary Functions:
 *      .area: calculate the area of the shape
 *      .dimension: calculate the largest distance between any two vertices
 *      .center: calculate the bounding box center of the shape
 *      .centroid: calculate the geometry center of the shape
 * 
 * Binary Operations:
 *      .distance: calculate the (minimum) distance between two shapes
 *      .max_distance: calculate the (maximum) distance between two shapes.
 *      .intersect: calculate the intersection of the two shapes
 *      .merge: calculate the shape with minimum area that contains the two shapes
 *
 * Extension Operations:
 *      .iou: calculate the intersection over union (about area)
 *      .giou: calculate the generalized intersection over union
 *          (ref "Generalized Intersection over Union")
 *      .diou: calculate the distance intersection over union
 *          (ref "Distance-IoU Loss: Faster and Better Learning for Bounding Box Regression")
 * 
 * Note that the library haven't been tested for stability, parallel edge and vertex on edge
 * should be avoided! This is due to both the complex implementation and incompatible gradient calculation.
 */

#ifndef DGAL_GEOMETRY_HPP
#define DGAL_GEOMETRY_HPP

#include <cmath>
#include <limits>
#include <string>
#include <sstream>
#include <type_traits>

// CUDA function descriptors
#ifdef __CUDACC__
#define CUDA_CALLABLE_MEMBER __host__ __device__
#define CUDA_RESTRICT __restrict__
#define CUDA_CONSTANT __constant__
#else
#define CUDA_CALLABLE_MEMBER
#define CUDA_RESTRICT
#define CUDA_CONSTANT
#endif 

namespace dgal {

//////////////////// forward declarations //////////////////////
template <typename scalar_t> struct Point2;
template <typename scalar_t> struct Line2;
template <typename scalar_t> struct Segment2;
template <typename scalar_t> struct AABox2;
template <typename scalar_t, uint8_t MaxPoints> struct Poly2;
template <typename scalar_t> using Quad2 = Poly2<scalar_t, 4>;

using Point2f = Point2<float>;
using Point2d = Point2<double>;
using Line2f = Line2<float>;
using Line2d = Line2<double>;
using Segment2f = Segment2<float>;
using Segment2d = Segment2<double>;
using AABox2f = AABox2<float>;
using AABox2d = AABox2<double>;
template <uint8_t MaxPoints> using Poly2f = Poly2<float, MaxPoints>;
template <uint8_t MaxPoints> using Poly2d = Poly2<double, MaxPoints>;
using Box2f = Quad2<float>;
using Box2d = Quad2<double>;

////////////////////////// Helpers //////////////////////////
template <typename T> class Numeric
{ 
    public:
        static constexpr T eps();
        static constexpr char tchar();
};
template <> class Numeric<float>
{ 
    public:
        static constexpr float eps() {return 3e-7;}
        static constexpr char tchar() {return 'f';}
};
template <> class Numeric<double>
{
    public:
        static constexpr double eps() {return 6e-15;}
        static constexpr char tchar() {return 'd';}
};

// Define the algorithms for selection
enum class Algorithm : int
{
    Default = 0,
    RotatingCaliper = 1,
    SutherlandHodgeman = 2
};

// Since partial template specialization for function is not allowed
// we tweak the overloading for selecting algorithm
struct AlgorithmT
{
    using Default = std::integral_constant<Algorithm, Algorithm::Default>; // default or undefined
    using RotatingCaliper = std::integral_constant<Algorithm, Algorithm::RotatingCaliper>;
    using SutherlandHodgeman = std::integral_constant<Algorithm, Algorithm::SutherlandHodgeman>;
};

constexpr double _pi = 3.14159265358979323846;

template <typename T> constexpr CUDA_CALLABLE_MEMBER inline
T _max(const T &a, const T &b) { return a > b ? a : b; }
template <typename T> constexpr CUDA_CALLABLE_MEMBER inline
T _min(const T &a, const T &b) { return a < b ? a : b; }
template <typename T> constexpr CUDA_CALLABLE_MEMBER inline
T _mod_inc(const T &i, const T &n) { return (i < n - 1) ? (i + 1) : 0; }
template <typename T> constexpr CUDA_CALLABLE_MEMBER inline
T _mod_dec(const T &i, const T &n) { return (i > 0) ? (i - 1) : (n - 1); }

///////////////////// implementations /////////////////////
template <typename scalar_t> struct Point2 // Point in 2D surface
{
    scalar_t x = 0, y = 0;

    // this operator is intended for gradient accumulation, rather than point offset,
    // which should be represented by a vector instead of a point
    CUDA_CALLABLE_MEMBER Point2<scalar_t>& operator+= (const Point2<scalar_t>& rhs)
    {
        x += rhs.x; y += rhs.y;
        return *this;
    }
};

// Calculate cross product (or area) of vector p1->p2 and p2->t
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t _cross(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2, const Point2<scalar_t> &t)
{
    return (p2.x - p1.x) * (t.y - p2.y) - (p2.y - p1.y) * (t.x - p2.x);
}

template <typename scalar_t> struct Line2 // Infinite but directional line
{
    scalar_t a = 0, b = 0, c = 0; // a*x + b*y + c = 0

    CUDA_CALLABLE_MEMBER inline bool intersects(const Line2<scalar_t> &l) const
    {
        return abs(a*l.b - l.a*b) > Numeric<scalar_t>::eps();
    }
};

template <typename scalar_t> struct Segment2
{
    scalar_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
};

template <typename scalar_t> struct AABox2 // Axis-aligned 2D box for quick calculation
{
    // contract: max_x >= min_x, max_y >= min_y
    scalar_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;

    CUDA_CALLABLE_MEMBER inline bool contains(const Point2<scalar_t>& p) const
    {
        return p.x > min_x && p.x < max_x && p.y > min_y && p.y < max_y;
    }

    CUDA_CALLABLE_MEMBER inline bool contains(const AABox2<scalar_t>& a) const
    {
        return max_x > a.max_x && min_x < a.min_x && max_y > a.max_y && min_y < a.min_y;
    }

    CUDA_CALLABLE_MEMBER inline bool intersects(const AABox2<scalar_t>& a) const
    {
        return max_x > a.min_x && min_x < a.max_x && max_y > a.min_y && min_y < a.max_y;
    }
};

template <typename scalar_t, uint8_t MaxPoints> struct Poly2 // Convex polygon with no holes
{
    // contract: points are already sorted in counter-clockwise order
    // contract: the points count <= 128
    Point2<scalar_t> vertices[MaxPoints];
    uint8_t nvertices = 0; // actual number of vertices

    template <uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER
    Poly2<scalar_t, MaxPoints>& operator=(const Poly2<scalar_t, MaxPoints2> &other)
    {
        assert(other.nvertices <= MaxPoints);
        nvertices = other.nvertices;
        for (uint8_t i = 0; i < other.nvertices; i++)
            vertices[i] = other.vertices[i];
        return *this;
    }

    CUDA_CALLABLE_MEMBER inline bool contains(const Point2<scalar_t>& p) const
    { 
        // deal with head and tail first
        if (_cross(vertices[nvertices-1], vertices[0], p) < 0) return false;

        // then loop remaining edges
        for (uint8_t i = 1; i < nvertices; i++)
            if (_cross(vertices[i-1], vertices[i], p) < 0)
                return false;

        return true;
    }

    // initialize the point values to zeros
    CUDA_CALLABLE_MEMBER inline void zero()
    {
        for (uint8_t i = 0; i < MaxPoints; i++)
        {
            vertices[i].x = 0;
            vertices[i].y = 0;
        }
    }
};

////////////////// print utilities (only available in CPU) //////////////////

template <typename scalar_t>
std::string to_string (const Point2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "(" << val.x << ", " << val.y << ")";
    return ss.str();
}

template <typename scalar_t>
std::string to_string (const Line2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "(a=" << val.a << ", b=" << val.b << ", c=" << val.c << ")";
    return ss.str(); 
}

template <typename scalar_t>
std::string to_string (const Segment2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "(" << val.x1 << "," << val.y1 << " -> " << val.x2 << "," << val.y2 << ")";
    return ss.str(); 
}

template <typename scalar_t>
std::string to_string (const AABox2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "(x: " << val.min_x << " ~ " << val.max_x << ", y: " << val.min_y << " ~ " << val.max_y << ")";
    return ss.str();
}

template <typename scalar_t, uint8_t MaxPoints>
std::string to_string (const Poly2<scalar_t, MaxPoints> &val)
{
    std::stringstream ss;
    ss << "[" << to_string(val.vertices[0]);
    for (uint8_t i = 1; i < val.nvertices; i++)
        ss << ", " << to_string(val.vertices[i]);
    ss << "]";
    return ss.str();
}

template <typename scalar_t>
std::string pprint (const Point2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "<Point2" << Numeric<scalar_t>::tchar() << ' ' << to_string(val) << '>';
    return ss.str();
}


template <typename scalar_t>
std::string pprint (const Line2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "<Line2" << Numeric<scalar_t>::tchar() << ' ' << to_string(val) << '>';
    return ss.str();
}

template <typename scalar_t>
std::string pprint (const Segment2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "<Segment2" << Numeric<scalar_t>::tchar() << ' ' << to_string(val) << '>';
    return ss.str();
}

template <typename scalar_t>
std::string pprint (const AABox2<scalar_t> &val)
{
    std::stringstream ss;
    ss << "<AABox2" << Numeric<scalar_t>::tchar() << ' ' << to_string(val) << '>';
    return ss.str();
}

template <typename scalar_t, uint8_t MaxPoints>
std::string pprint (const Poly2<scalar_t, MaxPoints> &val)
{
    std::stringstream ss;
    ss << "<Poly2" << Numeric<scalar_t>::tchar() << MaxPoints << ' ' << to_string(val) << '>';
    return ss.str();
}

////////////////// constructors ///////////////////

// Note that the order of points matters
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Line2<scalar_t> line2_from_xyxy(const scalar_t &x1, const scalar_t &y1,
    const scalar_t &x2, const scalar_t &y2)
{
    return {.a=y2-y1, .b=x1-x2, .c=x2*y1-x1*y2};
}

// Note that the order of points matters
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Line2<scalar_t> line2_from_pp(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2)
{
    return line2_from_xyxy(p1.x, p1.y, p2.x, p2.y);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Segment2<scalar_t> segment2_from_pp(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2)
{
    return {.x1=p1.x, .y1=p1.y, .x2=p2.x, .y2=p2.y};
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Line2<scalar_t> line2_from_segment2(const Segment2<scalar_t> &s)
{
    return line2_from_xyxy(s.x1, s.y1, s.x2, s.y2);
}

// Return the point on line l represented by single parameter t
// Reference: CGAL::Line_2<Kernel>::point(const Kernel::FT i)
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Point2<scalar_t> point_from_t(const Line2<scalar_t> &l, const scalar_t &t)
{
    if (l.b == 0)
        return { .x = l.c / l.a, .y = 1 - t * l.a };
    else
        return {
            .x = 1 + t * l.b,
            .y = -(l.a+l.c)/l.b - t * l.a
        };
}

// Return the single parameter representation (wrt. line l) of the projection of a point to line
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t t_from_pxy(const Line2<scalar_t> &l, const scalar_t &x, const scalar_t &y)
{
    if (l.b == 0)
        return (1 - y) / l.a;
    else if (l.a == 0)
        return (x - 1) / l.b;
    else
        return (l.b*x - l.a*y - l.a*(l.a+l.c)/l.b - l.b) / (l.a*l.a + l.b*l.b);
}
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t t_from_ppoint(const Line2<scalar_t> &l, const Point2<scalar_t> &p)
{ return t_from_pxy(l, p.x, p.y); }

// convert AABox2 to a polygon (box)
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, 4> poly2_from_aabox2(const AABox2<scalar_t> &a)
{
    Point2<scalar_t> p1{a.min_x, a.min_y},
        p2{a.max_x, a.min_y},
        p3{a.max_x, a.max_y},
        p4{a.min_x, a.max_y};
    return {.vertices={p1, p2, p3, p4}, .nvertices=4};
}

// calculate bounding box of a polygon
template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
AABox2<scalar_t> aabox2_from_poly2(const Poly2<scalar_t, MaxPoints> &p)
{
    AABox2<scalar_t> result {
        .min_x = p.vertices[0].x, .max_x = p.vertices[0].x,
        .min_y = p.vertices[0].y, .max_y = p.vertices[0].y
    };

    for (uint8_t i = 1; i < p.nvertices; i++)
    {
        result.min_x = _min(p.vertices[i].x, result.min_x);
        result.max_x = _max(p.vertices[i].x, result.max_x);
        result.min_y = _min(p.vertices[i].y, result.min_y);
        result.max_y = _max(p.vertices[i].y, result.max_y);
    }
    return result;
}

// Create a polygon from box parameters (x, y, width, height, rotation)
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, 4> poly2_from_xywhr(const scalar_t& x, const scalar_t& y,
    const scalar_t& w, const scalar_t& h, const scalar_t& r)
{
    scalar_t dxsin = w*sin(r)/2, dxcos = w*cos(r)/2;
    scalar_t dysin = h*sin(r)/2, dycos = h*cos(r)/2;

    Point2<scalar_t> p0 {.x = x - dxcos + dysin, .y = y - dxsin - dycos};
    Point2<scalar_t> p1 {.x = x + dxcos + dysin, .y = y + dxsin - dycos};
    Point2<scalar_t> p2 {.x = x + dxcos - dysin, .y = y + dxsin + dycos};
    Point2<scalar_t> p3 {.x = x - dxcos - dysin, .y = y - dxsin + dycos};
    return {.vertices={p0, p1, p2, p3}, .nvertices=4};
}

//////////////////// functions ///////////////////

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2)
{
    return hypot(p1.x - p2.x, p1.y - p2.y);
}

// Calculate signed distance from point p to the line
// The distance is negative if the point is at left hand side wrt the direction of line (x1y1 -> x2y2)
// Note: this behavior is the opposite to the cross product
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Line2<scalar_t> &l, const Point2<scalar_t> &p)
{
    return (l.a*p.x + l.b*p.y + l.c) / hypot(l.a, l.b);
}
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Point2<scalar_t> &p, const Line2<scalar_t> &l)
{ return distance(l, p); }

// Calculate signed distance from point p to the segment s
// Sign assignment is the same with distance from point to line
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Segment2<scalar_t> &s, const Point2<scalar_t> &p)
{
    Line2<scalar_t> l = line2_from_segment2(s);
    scalar_t t = t_from_ppoint(l, p);
    scalar_t sign = l.a*p.x + l.b*p.y + l.c;

    if (t < t_from_pxy(l, s.x2, s.y2))
    {
        Point2<scalar_t> p2 {.x=s.x2, .y=s.y2};
        scalar_t d = distance(p, p2);
        return sign > 0 ? d : -d;
    }
    else if(t > t_from_pxy(l, s.x1, s.y1))
    {
        Point2<scalar_t> p1 {.x=s.x1, .y=s.y1};
        scalar_t d = distance(p, p1);
        return sign > 0 ? d : -d;
    }
    else
        return sign / hypot(l.a, l.b);
}
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Point2<scalar_t> &p, const Segment2<scalar_t> &s)
{ return distance(s, p); }

// Calculate signed distance from point p to the polygon poly
// The distance is positive if the point is inside the polygon
// The index corresponds to an edge if the point is inside the polygon, correspond to an edge or a vertex if outside
template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Poly2<scalar_t, MaxPoints> &poly, const Point2<scalar_t> &p, uint8_t &idx)
{
    scalar_t dmin = -distance(segment2_from_pp(poly.vertices[poly.nvertices-1], poly.vertices[0]), p);
    idx = poly.nvertices - 1;
    for (uint8_t i = 1; i < poly.nvertices; i++)
    {
        scalar_t dl = -distance(segment2_from_pp(poly.vertices[i-1], poly.vertices[i]), p);
        if (abs(dl) < abs(dmin))
        {
            dmin = dl;
            idx = i-1;
        }
    }
    return dmin;
}
template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Poly2<scalar_t, MaxPoints> &poly, const Point2<scalar_t> &p)
{ uint8_t index; return distance(poly, p, index); }
template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
scalar_t distance(const Point2<scalar_t> &p, const Poly2<scalar_t, MaxPoints> &poly)
{ uint8_t index; return distance(poly, p, index); }

// Calculate intersection point of two lines
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Point2<scalar_t> intersect(const Line2<scalar_t> &l1, const Line2<scalar_t> &l2)
{
    scalar_t w = l1.a*l2.b - l2.a*l1.b;
    return {.x = (l1.b*l2.c - l2.b*l1.c) / w, .y = (l1.c*l2.a - l2.c*l1.a) / w};
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
AABox2<scalar_t> intersect(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2)
{
    if (a1.max_x <= a2.min_x || a1.min_x >= a2.max_x ||
        a1.max_y <= a2.min_y || a1.min_y >= a2.max_y)
        // return empty aabox if no intersection
        return {0, 0, 0, 0};
    else
    {
        return {
            .min_x = _max(a1.min_x, a2.min_x),
            .max_x = _min(a1.max_x, a2.max_x),
            .min_y = _max(a1.min_y, a2.min_y),
            .max_y = _min(a1.max_y, a2.max_y)
        };
    }
}

// Find the intersection point under a bridge over two convex polygons
// p1 is searched from idx1 in clockwise order and p2 is searched from idx2 in counter-clockwise order
// the indices of edges of the intersection are reported by xidx1 and xidx2
// note: index of an edge is the index of its (counter-clockwise) starting point
// If intersection is not found, then false will be returned
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
bool _find_intersection_under_bridge(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const uint8_t &idx1, const uint8_t &idx2, uint8_t &xidx1, uint8_t &xidx2)
{
    uint8_t i1 = idx1, i2 = idx2;
    bool finished = false;
    scalar_t dist, last_dist; // here we use cross product to represent distance

    while (!finished)
    { 
        finished = true;

        // traverse down along p2
        last_dist = -std::numeric_limits<scalar_t>::infinity();
        const Point2<scalar_t> &p1a = p1.vertices[_mod_dec(i1, p1.nvertices)];
        const Point2<scalar_t> &p1b = p1.vertices[i1];
        while (true)
        {
            dist = _cross(p1a, p1b, p2.vertices[_mod_inc(i2, p2.nvertices)]);
            //DEBUG printf("bridge down along _p2 (%d), cross: %.7f\n", _mod_inc(i2, p2.nvertices), dist);
            if (dist < last_dist) return false;
            if (dist > -Numeric<scalar_t>::eps()) break;

            i2 = _mod_inc(i2, p2.nvertices);
            last_dist = dist;
            finished = false;
        }

        // traverse down along p1
        last_dist = -std::numeric_limits<scalar_t>::infinity();
        const Point2<scalar_t> &p2a = p2.vertices[i2];
        const Point2<scalar_t> &p2b = p2.vertices[_mod_inc(i2, p2.nvertices)];
        while (true)
        {
            dist = _cross(p2a, p2b, p1.vertices[_mod_dec(i1, p1.nvertices)]);
            //DEBUG printf("bridge down along _p1 (%d), cross: %.7f\n", _mod_dec(i1, p1.nvertices), dist);
            if (dist < last_dist) return false;
            if (dist > -Numeric<scalar_t>::eps()) break;

            i1 = _mod_dec(i1, p1.nvertices);
            last_dist = dist;
            finished = false;
        }
    }

    // TODO: maybe need to test wether the last two segments actually intersect
    xidx1 = _mod_dec(i1, p1.nvertices);
    xidx2 = i2;
    return true;
}

// Find the extreme vertex in a polygon
// TopRight=true is finding point with biggest y
// TopRight=false if finding point with smallest y
// Right now the case where there's multiple points with extreme y is ignored since we assume non-singularity
template <typename scalar_t, uint8_t MaxPoints, bool TopRight=true> CUDA_CALLABLE_MEMBER inline
void _find_extreme(const Poly2<scalar_t, MaxPoints> &p, uint8_t &idx, scalar_t &ey)
{
    idx = 0;
    ey = p.vertices[0].y;
    for (uint8_t i = 1; i < p.nvertices; i++)
        if ((TopRight && p.vertices[i].y > ey) || (!TopRight && p.vertices[i].y < ey))
        {
            ey = p.vertices[i].y;
            idx = i;
        }
}

// Calculate slope angle from p1 to p2
template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t _slope_pp(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2)
{
    return atan2(p2.y - p1.y - Numeric<scalar_t>::eps(), p2.x - p1.x); // eps is used to let atan2(0, -1)=-pi instead of pi
}

// check if the bridge defined between two polygon (from p1 to p2) is valid.
// reverse is set to true if the polygons lay in the right of the bridge
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
bool _check_valid_bridge(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const uint8_t &idx1, const uint8_t &idx2, bool &reverse)
{
    scalar_t d1 = _cross(p1.vertices[idx1], p2.vertices[idx2], p1.vertices[_mod_dec(idx1, p1.nvertices)]);
    scalar_t d2 = _cross(p1.vertices[idx1], p2.vertices[idx2], p1.vertices[_mod_inc(idx1, p1.nvertices)]);
    scalar_t d3 = _cross(p1.vertices[idx1], p2.vertices[idx2], p2.vertices[_mod_dec(idx2, p2.nvertices)]);
    scalar_t d4 = _cross(p1.vertices[idx1], p2.vertices[idx2], p2.vertices[_mod_inc(idx2, p2.nvertices)]);
    //DEBUG printf("bridge test: %.7f\t %.7f\t %.7f\t %.7f\n", d1, d2, d3, d4);

    bool found = false;
    if (abs(d1) > Numeric<scalar_t>::eps())
    {
        found = true;
        reverse = d1 < 0;
    }
    if (abs(d2) > Numeric<scalar_t>::eps())
    {
        if (found)
        {
            if (reverse != (d2 < 0))
            return false;
        }
        else
        {
            found = true;
            reverse = d2 < 0;
        }
    }
    if (abs(d3) > Numeric<scalar_t>::eps())
    {
        if (found)
        {
            if (reverse != (d3 < 0))
            return false;
        }
        else
        {
            found = true;
            reverse = d3 < 0;
        }
    }
    if (abs(d4) > Numeric<scalar_t>::eps())
    {
        if (found)
        {
            if (reverse != (d4 < 0))
            return false;
        }
        else
        {
            found = true;
            reverse = d4 < 0;
        }
    }

    if (!found) assert(false); // all adjacent points are on the same line!
    return true;
}

// xflags stores edges flags for gradient computation:
// left 7bits from the 8bits are index number of the edge (index of the first vertex of edge)
// right 1 bit indicate whether edge is from p1 (=1) or p2 (=0)
// the vertices of the output polygon are ordered so that vertex i is the intersection of edge i-1 and edge i in the flags
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> intersect(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t xflags[MaxPoints1 + MaxPoints2] = nullptr
);

// Rotating Caliper implementation of intersecting
// Reference: https://web.archive.org/web/20150415231115/http://cgm.cs.mcgill.ca/~orm/rotcal.frame.html
//    and http://cgm.cs.mcgill.ca/~godfried/teaching/cg-projects/97/Plante/CompGeomProject-EPlante/algorithm.html
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> intersect(AlgorithmT::RotatingCaliper,
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t xflags[MaxPoints1 + MaxPoints2] = nullptr
) {
    // find the vertices with max y value, starting from line pointing to -x (angle is -pi)
    uint8_t pidx1, pidx2; scalar_t _;
    _find_extreme(p1, pidx1, _);
    _find_extreme(p2, pidx2, _);

    // start rotating to find all intersection points
    scalar_t edge_angle = -_pi; // scan from -pi to pi
    bool edge_flag; // true: intersection connection will start with p1, false: start with p2
    bool reverse; // temporary var
    uint8_t nx = 0; // number of intersection points
    uint8_t x1indices[MaxPoints1 + MaxPoints2 + 1], x2indices[MaxPoints1 + MaxPoints2 + 1];

    while (true)
    {
        uint8_t pidx1_next = _mod_inc(pidx1, p1.nvertices);
        uint8_t pidx2_next = _mod_inc(pidx2, p2.nvertices);
        scalar_t angle1 = _slope_pp(p1.vertices[pidx1], p1.vertices[pidx1_next]);
        scalar_t angle2 = _slope_pp(p2.vertices[pidx2], p2.vertices[pidx2_next]);

        // compare angles and proceed
        if (edge_angle < angle1 && (angle1 < angle2 || angle2 < edge_angle))
        { // choose p1 edge as part of co-podal pair

            //DEBUG printf("(p1@%d, p2@%d) ", pidx1_next, pidx2);
            if (_check_valid_bridge(p1, p2, pidx1_next, pidx2, reverse)) // valid bridge
            {
                bool has_intersection = reverse ?
                    _find_intersection_under_bridge(p1, p2, pidx1_next, pidx2, x1indices[nx], x2indices[nx]):
                    _find_intersection_under_bridge(p2, p1, pidx2, pidx1_next, x2indices[nx], x1indices[nx]);
                if (!has_intersection)
                    return {}; // return empty polygon
                
                // save intersection
                if (nx == 0) edge_flag = !reverse;
                //DEBUG printf("add intersection: pidx1=%d, pidx2=%d\n", x1indices[nx], x2indices[nx]);
                nx++;
            }

            // update pointer
            pidx1 = _mod_inc(pidx1, p1.nvertices);
            edge_angle = angle1;
        }
        else if (edge_angle < angle2 && (angle2 < angle1 || angle1 < edge_angle))
        { // choose p2 edge as part of co-podal pair

            //DEBUG printf("(p1@%d, p2@%d) ", pidx1, pidx2_next);
            if (_check_valid_bridge(p1, p2, pidx1, pidx2_next, reverse)) // valid bridge
            {
                bool has_intersection = reverse ?
                    _find_intersection_under_bridge(p1, p2, pidx1, pidx2_next, x1indices[nx], x2indices[nx]):
                    _find_intersection_under_bridge(p2, p1, pidx2_next, pidx1, x2indices[nx], x1indices[nx]);
                if (!has_intersection)
                    return {}; // return empty polygon
                
                // save intersection
                if (nx == 0) edge_flag = !reverse;
                //DEBUG printf("add intersection: pidx1=%d, pidx2=%d\n", x1indices[nx], x2indices[nx]);
                nx++;
            }

            // update pointer
            pidx2 = _mod_inc(pidx2, p2.nvertices);
            edge_angle = angle2;
        }
        else break; // when both angles are not increasing, the loop is finished
    }

    // if no intersection found but didn't return early (no bridge), then containment is detected
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> result;
    if (nx == 0)
    {
        if (area(p1) > area(p2)) // use area to determine which one is inside
        {
            result = p2;
            if (xflags != nullptr)
                for (uint8_t i = 0; i < p2.nvertices; i++)
                    xflags[i] = i << 1;
        }
        else
        {
            result = p1;
            if (xflags != nullptr)
                for (uint8_t i = 0; i < p1.nvertices; i++)
                    xflags[i] = (i << 1) | 1;
        }
        return result;
    }

    // loop over the intersections to construct the result polygon
    //DEBUG printf("%d\n", nx);

    x1indices[nx] = x1indices[0]; // add sentinels
    x2indices[nx] = x2indices[0];
    for (uint8_t i = 0; i < nx; i++)
    {
        // add intersection point
        Line2<scalar_t> l1 = line2_from_pp(p1.vertices[x1indices[i]], p1.vertices[_mod_inc(x1indices[i], p1.nvertices)]);
        Line2<scalar_t> l2 = line2_from_pp(p2.vertices[x2indices[i]], p2.vertices[_mod_inc(x2indices[i], p2.nvertices)]);
        if (xflags != nullptr)
            xflags[result.nvertices] = edge_flag ? (x1indices[i] << 1 | 1) : (x2indices[i] << 1);
        result.vertices[result.nvertices++] = intersect(l1, l2);
        assert(result.nvertices <= MaxPoints1 + MaxPoints2);
        //DEBUG printf("%d, add cross points\n", result.nvertices);

        // add points between intersections
        if (edge_flag)
        {
            //DEBUG printf("[Debug] x1: ");
            //DEBUG for (uint8_t x = 0; x <= nx; x++)
            //DEBUG     printf("%d ", x1indices[x]);
            //DEBUG printf("\n[Debug] x2: ");
            //DEBUG for (uint8_t x = 0; x <= nx; x++)
            //DEBUG     printf("%d ", x2indices[x]);
            //DEBUG printf("\n");
            uint8_t idx_next = x1indices[i+1] >= x1indices[i] ? x1indices[i+1] : (x1indices[i+1] + p1.nvertices);
            //DEBUG printf("add p1 points from %d to %d\n", x1indices[i] + 1, idx_next);
            for (uint8_t j = x1indices[i] + 1; j <= idx_next; j++)
            {
                uint8_t jmod = j < p1.nvertices ? j : (j - p1.nvertices);
                if (xflags != nullptr)
                    xflags[result.nvertices] = (jmod << 1) | 1;
                result.vertices[result.nvertices++] = p1.vertices[jmod];
                assert(result.nvertices <= MaxPoints1 + MaxPoints2);
                //DEBUG printf("%d, add p1 points\n", result.nvertices);
            }
        }
        else
        {
            //DEBUG printf("[Debug] x1: ");
            //DEBUG for (uint8_t x = 0; x <= nx; x++)
            //DEBUG     printf("%d ", x1indices[x]);
            //DEBUG printf("\n[Debug] x2: ");
            //DEBUG for (uint8_t x = 0; x <= nx; x++)
            //DEBUG     printf("%d ", x2indices[x]);
            //DEBUG printf("\n");
            uint8_t idx_next = x2indices[i+1] >= x2indices[i] ? x2indices[i+1] : (x2indices[i+1] + p2.nvertices);
            //DEBUG printf("add p2 points from %d to %d\n", x2indices[i] + 1, idx_next);
            for (uint8_t j = x2indices[i] + 1; j <= idx_next; j++)
            {
                uint8_t jmod = j < p2.nvertices ? j : (j - p2.nvertices);
                if (xflags != nullptr)
                    xflags[result.nvertices] = jmod << 1;
                result.vertices[result.nvertices++] = p2.vertices[jmod];
                assert(result.nvertices <= MaxPoints1 + MaxPoints2);
                //DEBUG printf("%d, add p2 points\n", result.nvertices);
            }
        }
        edge_flag = !edge_flag;
    }

    return result;
}

// This algorithm is the simplest one, but it's actually O(N*M) complexity
// For more efficient algorithms refer to Rotating Calipers, Sweep Line, etc
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> intersect(AlgorithmT::SutherlandHodgeman,
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t xflags[MaxPoints1 + MaxPoints2] = nullptr
) {
    using PolyT = Poly2<scalar_t, MaxPoints1 + MaxPoints2>;
    PolyT temp1, temp2; // declare variables to store temporary results
    PolyT *pcut = &(temp1 = p1), *pcur = &temp2; // start with original polygon

    uint8_t flag1[MaxPoints1 + MaxPoints2], flag2[MaxPoints1 + MaxPoints2];
    for (uint8_t i = 0; i < p1.nvertices; i++)
        flag1[i] = i << 1 | 1;
    uint8_t *fcut = flag1, *fcur = flag2;

    for (uint8_t j = 0; j < p2.nvertices; j++) // loop over edges of polygon doing cut
    {
        uint8_t jnext = _mod_inc(j, p2.nvertices);
        auto edge = line2_from_pp(p2.vertices[j], p2.vertices[jnext]);

        scalar_t signs[MaxPoints1 + MaxPoints2];
        for (uint8_t i = 0; i < pcut->nvertices; i++)
            signs[i] = distance(edge, pcut->vertices[i]);

        for (uint8_t i = 0; i < pcut->nvertices; i++) // loop over edges of polygon to be cut
        {
            if (signs[i] < Numeric<scalar_t>::eps()) // eps is used for numerical stable when the boxes are very close
            {
                pcur->vertices[pcur->nvertices] = pcut->vertices[i];
                fcur[pcur->nvertices] = fcut[i];
                pcur->nvertices++;
            }

            uint8_t inext = _mod_inc(i, pcut->nvertices);
            if (signs[i] * signs[inext] < -Numeric<scalar_t>::eps())
            {
                auto cut = line2_from_pp(pcut->vertices[i], pcut->vertices[inext]);
                pcur->vertices[pcur->nvertices] = intersect(edge, cut);
                if (signs[i] < -Numeric<scalar_t>::eps())
                    fcur[pcur->nvertices] = j << 1;
                else
                    fcur[pcur->nvertices] = fcut[i];
                pcur->nvertices++;
            }
        }

        PolyT* p = pcut; pcut = pcur; pcur = p; // swap polygon
        uint8_t * f = fcut; fcut = fcur; fcur = f; // swap flags
        pcur->nvertices = 0; // clear current polygon for next iteration
    }

    if (xflags != nullptr)
    {
        for (uint8_t i = 0; i < pcut->nvertices; i++)
            xflags[i] = fcut[i];
    }
    return *pcut;
}

// default implementation
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> intersect(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t xflags[MaxPoints1 + MaxPoints2]
) {
    // TODO: Sutherland-Hodgeman might be faster when the polygon has few edges
    return intersect(AlgorithmT::RotatingCaliper(), p1, p2, xflags);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t area(const AABox2<scalar_t> &a)
{
    return (a.max_x - a.min_x) * (a.max_y - a.min_y);
}

// Calculate inner area of a polygon
template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
scalar_t area(const Poly2<scalar_t, MaxPoints> &p)
{
    if (p.nvertices <= 2)
        return 0;

    scalar_t sum = p.vertices[p.nvertices-1].x*p.vertices[0].y // deal with head and tail first
                 - p.vertices[p.nvertices-1].y*p.vertices[0].x;
    for (uint8_t i = 1; i < p.nvertices; i++)
        sum += p.vertices[i-1].x*p.vertices[i].y - p.vertices[i].x*p.vertices[i-1].y;
    return sum / 2;
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t dimension(const AABox2<scalar_t> &a)
{
    return hypot(a.max_x - a.min_x, a.max_y - a.min_y);
}

// use Rotating Caliper to find the dimension
template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
scalar_t dimension(const Poly2<scalar_t, MaxPoints> &p, uint8_t &flag1, uint8_t &flag2)
{
    if (p.nvertices <= 1) return 0;
    if (p.nvertices == 2)
    {
        flag1 = 0; flag2 = 1;
        return distance(p.vertices[0], p.vertices[1]);
    }

    uint8_t u = 0, unext = 1, v = 1, vnext = 2;
    scalar_t dmax = 0, d;
    for (u = 0; u < p.nvertices; u++)
    {
        // find farthest point from current edge
        unext = _mod_inc(u, p.nvertices);
        while (_cross(p.vertices[u], p.vertices[unext], p.vertices[v]) <=
               _cross(p.vertices[u], p.vertices[unext], p.vertices[vnext])
        ) {
            v = vnext;
            vnext = _mod_inc(v, p.nvertices);
        }

        // update max value
        d = distance(p.vertices[u], p.vertices[v]);
        if (d > dmax)
        {
            dmax = d;
            flag1 = u;
            flag2 = v;
        }
        
        d = distance(p.vertices[unext], p.vertices[v]);
        if (d > dmax)
        {
            dmax = d;
            flag1 = unext;
            flag2 = v;
        }
    }
    return dmax;
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
scalar_t dimension(const Poly2<scalar_t, MaxPoints> &p)
{ uint8_t _; return dimension(p, _, _); }

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Point2<scalar_t> center(const AABox2<scalar_t> &a)
{
    return {.x = (a.max_x + a.min_x)/2, .y = (a.max_y + a.min_y)/2};
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
Point2<scalar_t> center(const Poly2<scalar_t, MaxPoints> &p)
{
    return center(aabox2_from_poly2(p));
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
Point2<scalar_t> centroid(const AABox2<scalar_t> &a)
{
    return center(a);
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
Point2<scalar_t> centroid(const Poly2<scalar_t, MaxPoints> &p)
{
    Point2<scalar_t> result;
    for (uint8_t i = 0; i < p.nvertices; i++)
        result += p.vertices[i];
    result.x /= p.nvertices;
    result.y /= p.nvertices;
    return result;
}

// use Rotating Caliper to find the convex hull of two polygons
// xflags here store the vertices flag
// left 7 bits represent the index of vertex in original polygon and
// right 1 bit indicate whether the vertex is from p1(=1) or p2(=0)
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> merge(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    CUDA_RESTRICT uint8_t mflags[MaxPoints1 + MaxPoints2] = nullptr
) {
    // find the vertices with max y value, starting from line pointing to -x (angle is -pi)
    uint8_t pidx1, pidx2;
    scalar_t y_max1, y_max2;
    _find_extreme(p1, pidx1, y_max1);
    _find_extreme(p2, pidx2, y_max2);

    // declare variables and functions
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> result;
    scalar_t edge_angle = -_pi; // scan from -pi to pi
    bool _; // temp var
    bool edge_flag = y_max1 > y_max2; // true: current edge on p1 will be present in merged polygon,
                                      //false: current edge on p2 will be present in merged polygon

    const auto register_p1point = [&](uint8_t i1)
    {
        if (mflags != nullptr)
            mflags[result.nvertices] = (i1 << 1) | 1;
        result.vertices[result.nvertices++] = p1.vertices[i1];
    };
    const auto register_p2point = [&](uint8_t i2)
    {
        if (mflags != nullptr)
            mflags[result.nvertices] = i2 << 1;
        result.vertices[result.nvertices++] = p2.vertices[i2];
    };
    const auto register_point = [&](uint8_t i1, uint8_t i2)
    {
        if (edge_flag) register_p1point(i1);
        else register_p2point(i2);
    };

    // check if starting point already build a bridge and if so, find correct edge to start
    if (_check_valid_bridge(p1, p2, pidx1, pidx2, _))
    {
        if (edge_flag)
        {
            uint8_t pidx1_next = _mod_inc(pidx1, p1.nvertices);
            scalar_t angle_br = _slope_pp(p1.vertices[pidx1], p2.vertices[pidx2]);
            scalar_t angle1 = _slope_pp(p1.vertices[pidx1], p1.vertices[pidx1_next]);
            if (angle_br < angle1)
                edge_flag = false;
        }
        else
        {
            uint8_t pidx2_next = _mod_inc(pidx2, p2.nvertices);
            scalar_t angle_br = _slope_pp(p2.vertices[pidx2], p1.vertices[pidx1]);
            scalar_t angle2 =  _slope_pp(p2.vertices[pidx2], p2.vertices[pidx2_next]);
            if (angle_br < angle2)
                edge_flag = true;
        }
    }

    // start rotating
    while (true)
    {
        uint8_t pidx1_next = _mod_inc(pidx1, p1.nvertices);
        uint8_t pidx2_next = _mod_inc(pidx2, p2.nvertices);
        scalar_t angle1 = _slope_pp(p1.vertices[pidx1], p1.vertices[pidx1_next]);
        scalar_t angle2 = _slope_pp(p2.vertices[pidx2], p2.vertices[pidx2_next]);

        // compare angles and proceed
        if (edge_angle < angle1 && (angle1 < angle2 || angle2 < edge_angle))
        { // choose p1 edge as part of co-podal pair

            if (edge_flag) register_p1point(pidx1); // Add vertex on current edge

            if (_check_valid_bridge(p1, p2, pidx1_next, pidx2, _))
            {
                // valid bridge, add bridge point
                register_point(pidx1_next, pidx2);
                edge_flag = !edge_flag;
            }

            // update pointer
            pidx1 = _mod_inc(pidx1, p1.nvertices);
            edge_angle = angle1;
        }
        else if (edge_angle < angle2 && (angle2 < angle1 || angle1 < edge_angle))
        { // choose p2 edge as part of co-podal pair

            if (!edge_flag) register_p2point(pidx2); // Add vertex on current edge

            if (_check_valid_bridge(p1, p2, pidx1, pidx2_next, _))
            {
                // valid bridge, add bridge point
                register_point(pidx1, pidx2_next);
                edge_flag = !edge_flag;
            }

            // update pointer
            pidx2 = _mod_inc(pidx2, p2.nvertices);
            edge_angle = angle2;
        }
        else break; // when both angles are not increasing, the loop is finished
    }

    return result;
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
AABox2<scalar_t> merge(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2)
{
    return {
        .min_x = _min(a1.min_x, a2.min_x),
        .max_x = _max(a1.max_x, a2.max_x),
        .min_y = _min(a1.min_y, a2.min_y),
        .max_y = _max(a1.max_y, a2.max_y)
    };
}

// use rotating caliper to find max distance between polygons
// this function use cross products to compare distances which could be faster
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t max_distance(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t &flag1, uint8_t &flag2
) {
    uint8_t pidx1, pidx2;
    scalar_t y_max1, y_max2;
    _find_extreme<scalar_t, MaxPoints1, true>(p1, pidx1, y_max1);
    _find_extreme<scalar_t, MaxPoints2, false>(p2, pidx2, y_max2);

    // start rotating to find all intersection points
    scalar_t edge_angle = -_pi; // scan p1 from -pi to pi
    scalar_t dmax = distance(p1.vertices[pidx1], p2.vertices[pidx2]);
    flag1 = pidx1; flag2 = pidx2;
    while (true)
    {
        uint8_t pidx1_next = _mod_inc(pidx1, p1.nvertices);
        uint8_t pidx2_next = _mod_inc(pidx2, p2.nvertices);
        scalar_t angle1 = _slope_pp(p1.vertices[pidx1], p1.vertices[pidx1_next]);
        scalar_t angle2 = _slope_pp(p2.vertices[pidx2_next], p2.vertices[pidx2]);

         // compare angles and proceed
        if (edge_angle < angle1 && (angle1 < angle2 || angle2 < edge_angle))
        { // choose p1 edge as part of anti-podal pair

            scalar_t d = distance(p1.vertices[pidx1_next], p2.vertices[pidx2]);
            if (d > dmax)
            {
                flag1 = pidx1_next;
                dmax = d;
            }

            // update pointer
            pidx1 = _mod_inc(pidx1, p1.nvertices);
            edge_angle = angle1;
        }
        else if (edge_angle < angle2 && (angle2 < angle1 || angle1 < edge_angle))
        { // choose p2 edge as part of anti-podal pair

            scalar_t d = distance(p1.vertices[pidx1], p2.vertices[pidx2_next]);
            if (d > dmax)
            {
                flag2 = pidx2_next;
                dmax = d;
            }

            // update pointer
            pidx2 = _mod_inc(pidx2, p2.nvertices);
            edge_angle = angle2;
        }
        else break; // when both angles are not increasing, the loop is finished
    }

    return dmax;
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t max_distance(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2
) { uint8_t _; return max_distance(p1, p2, _, _); }

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t max_distance(const AABox2<scalar_t> &b1, const AABox2<scalar_t> &b2)
{ return dimension(merge(b1, b2)); }

///////////// Custom functions /////////////


template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t iou(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2)
{
    scalar_t area_i = area(intersect(a1, a2));
    scalar_t area_u = area(a1) + area(a2) - area_i;
    return area_i / area_u;
}

// calculating iou of two polygons. xflags is used in intersection caculation
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t iou(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t &nx, CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2] = nullptr)
{
    auto pi = intersect(p1, p2, xflags);
    nx = pi.nvertices;
    scalar_t area_i = area(pi);
    scalar_t area_u = area(p1) + area(p2) - area_i;
    return area_i / area_u;
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t iou(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2)
{
    uint8_t _; return iou(p1, p2, _, nullptr);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t giou(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2)
{
    scalar_t area_i = area(intersect(a1, a2));
    scalar_t area_m = area(merge(a1, a2));
    scalar_t area_u = area(a1) + area(a2) - area_i;
    return area_i / area_u + area_u / area_m - 1;
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t giou(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t &nx, uint8_t &nm, CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2] = nullptr,
    CUDA_RESTRICT uint8_t mflags[MaxPoints1 + MaxPoints2] = nullptr)
{
    auto pi = intersect(p1, p2, xflags);
    nx = pi.nvertices;
    auto pm = merge(p1, p2, mflags);
    nm = pm.nvertices;

    scalar_t area_i = area(pi);
    scalar_t area_m = area(pm);
    scalar_t area_u = area(p1) + area(p2) - area_i;
    return area_i / area_u + area_u / area_m - 1;
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t giou(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2)
{
    uint8_t _; return giou(p1, p2, _, _, nullptr, nullptr);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
scalar_t diou(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2)
{
    scalar_t piou = iou(a1, a2);
    scalar_t maxd = dimension(merge(a1, a2));
    scalar_t cd = distance(centroid(a1), centroid(a2));
    return piou - (cd*cd) / (maxd*maxd);
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t diou(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    uint8_t &nx, uint8_t &dflag1, uint8_t &dflag2,
    CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2] = nullptr)
{
    scalar_t piou = iou(p1, p2, nx, xflags);
    scalar_t cd = distance(centroid(p1), centroid(p2));

    uint8_t mflags[8], idx1, idx2;
    scalar_t maxd = dimension(merge(p1, p2, mflags), idx1, idx2);
    dflag1 = mflags[idx1]; dflag2 = mflags[idx2];

    return piou - (cd*cd) / (maxd*maxd);
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
scalar_t diou(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2)
{
    uint8_t _; return diou(p1, p2, _, _, _, nullptr);
}

} // namespace dgal

#endif // DGAL_GEOMETRY_HPP

/* DEPRICATED CODES (for backup)

// Sutherland-Hodgman https://stackoverflow.com/a/45268241/5960776
// This algorithm is the simplest one, but it's actually O(N*M) complexity
// For more efficient algorithms refer to Rotating Calipers, Sweep Line, etc
template <typename scalar_t, uint8_t MaxPoints, uint8_t MaxPointsOther> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints + MaxPointsOther> intersect(
    const Poly2<scalar_t, MaxPoints> &p1, const Poly2<scalar_t, MaxPointsOther> &p2
) {
    using PolyT = Poly2<scalar_t, MaxPoints + MaxPointsOther>;
    PolyT temp1, temp2; // declare variables to store temporary results
    PolyT *pcut = &(temp1 = p1), *pcur = &temp2; // start with original polygon

    // these flags are saved for backward computation
    // left 16bits for points from p1, right 16bits for points from p2
    // left 15bits from the 16bits are index number, right 1 bit indicate whether point from this polygon is used
    int16_t flag1[MaxPoints + MaxPointsOther], flag2[MaxPoints + MaxPointsOther];
    int16_t *fcut = flag1, *fcur = flag2;

    for (uint8_t j = 0; j < p2.nvertices; j++) // loop over edges of polygon doing cut
    {
        uint8_t jnext = _mod_inc(j, p2.nvertices);
        auto edge = line2_from_pp(p2.vertices[j], p2.vertices[jnext]);

        scalar_t signs[MaxPoints + MaxPointsOther];
        for (uint8_t i = 0; i < pcut->nvertices; i++)
            signs[i] = distance(edge, pcut->vertices[i]);

        for (uint8_t i = 0; i < pcut->nvertices; i++) // loop over edges of polygon to be cut
        {
            if (signs[i] < GEOMETRY_EPS) // eps is used for numerical stable when the boxes are very close
                pcur->vertices[pcur->nvertices++] = pcut->vertices[i];

            uint8_t inext = _mod_inc(i, pcut->nvertices);
            if (signs[i] * signs[inext] < -GEOMETRY_EPS)
            {
                auto cut = line2_from_pp(pcut->vertices[i], pcut->vertices[inext]);
                pcur->vertices[pcur->nvertices++] = intersect(edge, cut);
            }
        }

        PolyT* p = pcut; pcut = pcur; pcur = p; // swap
        pcur->nvertices = 0; // clear current polygon for next iteration
    }
    return *pcut;
}

END DEPRICATED CODES */
