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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "dgal/geometry.hpp"
#include "dgal/geometry_grad.hpp"

namespace py = pybind11;
using namespace std;
using namespace dgal;
using namespace pybind11::literals;
typedef double T;

template <typename scalar_t, uint8_t MaxPoints> inline
Poly2<scalar_t, MaxPoints> poly_from_points(const vector<Point2<scalar_t>> &points)
{
    Poly2<scalar_t, MaxPoints> b; assert(points.size() <= MaxPoints);
    b.nvertices = points.size();
    for (uint8_t i = 0; i < points.size(); i++)
        b.vertices[i] = points[i];
    return b;
}

PYBIND11_MODULE(dgal, m) {
    m.doc() = "Python binding of the builtin geometry library of dgal, mainly for testing";

    py::class_<Point2<T>>(m, "Point2")
        .def(py::init<>())
        .def(py::init<T, T>())
        .def_readwrite("x", &Point2<T>::x)
        .def_readwrite("y", &Point2<T>::y)
        .def("__str__", py::overload_cast<const Point2<T>&>(&dgal::to_string<T>))
        .def("__repr__", py::overload_cast<const Point2<T>&>(&dgal::pprint<T>));
    py::class_<Line2<T>>(m, "Line2")
        .def(py::init<>())
        .def(py::init<T, T, T>())
        .def_readwrite("a", &Line2<T>::a)
        .def_readwrite("b", &Line2<T>::b)
        .def_readwrite("c", &Line2<T>::c)
        .def("__str__", py::overload_cast<const Line2<T>&>(&dgal::to_string<T>))
        .def("__repr__", py::overload_cast<const Line2<T>&>(&dgal::pprint<T>));
    py::class_<Segment2<T>>(m, "Segment2")
        .def(py::init<>())
        .def(py::init<T, T, T, T>())
        .def_readwrite("x1", &Segment2<T>::x1)
        .def_readwrite("y1", &Segment2<T>::y1)
        .def_readwrite("x2", &Segment2<T>::x2)
        .def_readwrite("y2", &Segment2<T>::y2)
        .def("__str__", py::overload_cast<const Segment2<T>&>(&dgal::to_string<T>))
        .def("__repr__", py::overload_cast<const Segment2<T>&>(&dgal::pprint<T>));
    py::class_<AABox2<T>>(m, "AABox2")
        .def(py::init<>())
        .def(py::init<T, T, T, T>())
        .def_readwrite("min_x", &AABox2<T>::min_x)
        .def_readwrite("max_x", &AABox2<T>::max_x)
        .def_readwrite("min_y", &AABox2<T>::min_y)
        .def_readwrite("max_y", &AABox2<T>::max_y)
        .def("__str__", py::overload_cast<const AABox2<T>&>(&dgal::to_string<T>))
        .def("__repr__", py::overload_cast<const AABox2<T>&>(&dgal::pprint<T>));
    py::class_<Quad2<T>>(m, "Quad2")
        .def(py::init<>())
        .def(py::init(&poly_from_points<T, 4>))
        .def_readonly("nvertices", &Quad2<T>::nvertices)
        .def_property_readonly("vertices", [](const Quad2<T> &b) {
            return vector<Point2<T>>(b.vertices, b.vertices + b.nvertices);})
        .def("__str__", py::overload_cast<const Quad2<T>&>(&dgal::to_string<T, 4>))
        .def("__repr__", py::overload_cast<const Quad2<T>&>(&dgal::pprint<T, 4>));
    py::class_<Poly2<T, 8>>(m, "Poly28")
        .def(py::init<>())
        .def(py::init(&poly_from_points<T, 8>))
        .def_readonly("nvertices", &Poly2<T, 8>::nvertices)
        .def_property_readonly("vertices", [](const Poly2<T, 8> &p) {
            return vector<Point2<T>>(p.vertices, p.vertices + p.nvertices);})
        .def("__str__", py::overload_cast<const Poly2<T, 8>&>(&dgal::to_string<T, 8>))
        .def("__repr__", py::overload_cast<const Poly2<T, 8>&>(&dgal::pprint<T, 8>));

    py::enum_<Algorithm>(m, "Algorithm")
        .value("Default", dgal::Algorithm::Default)
        .value("RotatingCaliper", dgal::Algorithm::RotatingCaliper)
        .value("SutherlandHodgeman", dgal::Algorithm::SutherlandHodgeman)
        .export_values();

    // constructors

    m.def("line2_from_pp", &dgal::line2_from_pp<T>, "Create a line with two points");
    m.def("line2_from_xyxy", &dgal::line2_from_xyxy<T>, "Create a line with coordinate of two points");
    m.def("segment2_from_pp", &dgal::segment2_from_pp<T>, "Create a line segment with coordinate of two points");
    m.def("line2_from_segment2", &dgal::line2_from_segment2<T>, "Create a line from a line segment");
    m.def("point_from_t", &dgal::point_from_t<T>, "Find the point on a line with parameter t");
    m.def("t_from_ppoint", &dgal::t_from_ppoint<T>, "Get parameter t for a point projected on a line");
    m.def("aabox2_from_poly2", &dgal::aabox2_from_poly2<T, 4>, "Create bounding box of a polygon");
    m.def("aabox2_from_poly2", &dgal::aabox2_from_poly2<T, 8>, "Create bounding box of a polygon");
    m.def("poly2_from_aabox2", &dgal::poly2_from_aabox2<T>, "Convert axis aligned box to polygon representation");
    m.def("poly2_from_xywhr", &dgal::poly2_from_xywhr<T>, "Creat a box with specified box parameters");

    // functions

    m.def("area", py::overload_cast<const AABox2<T>&>(&dgal::area<T>), "Get the area of axis aligned box");
    m.def("area", py::overload_cast<const Quad2<T>&>(&dgal::area<T, 4>), "Get the area of box");
    m.def("area", py::overload_cast<const Poly2<T, 8>&>(&dgal::area<T, 8>), "Get the area of polygon");
    m.def("dimension", py::overload_cast<const AABox2<T>&>(&dgal::dimension<T>), "Get the dimension of axis aligned box");
    m.def("dimension", py::overload_cast<const Quad2<T>&>(&dgal::dimension<T, 4>), "Get the dimension of box");
    m.def("dimension_", [](const Quad2<T>& b){
        uint8_t i1, i2; T v = dgal::dimension(b, i1, i2);
        return make_tuple(v, i1, i2);
    }, "Get the dimension of box");
    m.def("dimension", py::overload_cast<const Poly2<T, 8>&>(&dgal::dimension<T, 8>), "Get the dimension of polygon");
    m.def("dimension_", [](const Poly2<T, 8>& b){
        uint8_t i1, i2; T v = dgal::dimension(b, i1, i2);
        return make_tuple(v, i1, i2);
    }, "Get the dimension of polygon");
    m.def("center", py::overload_cast<const AABox2<T>&>(&dgal::center<T>), "Get the center point of axis aligned box");
    m.def("center", py::overload_cast<const Quad2<T>&>(&dgal::center<T, 4>), "Get the center point of box");
    m.def("center", py::overload_cast<const Poly2<T, 8>&>(&dgal::center<T, 8>), "Get the center point of polygon");
    m.def("centroid", py::overload_cast<const AABox2<T>&>(&dgal::centroid<T>), "Get the centroid point of axis aligned box");
    m.def("centroid", py::overload_cast<const Quad2<T>&>(&dgal::centroid<T, 4>), "Get the centroid point of box");
    m.def("centroid", py::overload_cast<const Poly2<T, 8>&>(&dgal::centroid<T, 8>), "Get the centroid point of polygon");

    // operators

    m.def("distance", py::overload_cast<const Point2<T>&, const Point2<T>&>(&dgal::distance<T>),
        "Get the distance between two points");
    m.def("distance", py::overload_cast<const Line2<T>&, const Point2<T>&>(&dgal::distance<T>),
        "Get the distance from a point to a line");
    m.def("distance", py::overload_cast<const Point2<T>&, const Line2<T>&>(&dgal::distance<T>),
        "Get the distance from a point to a line");
    m.def("distance", py::overload_cast<const Segment2<T>&, const Point2<T>&>(&dgal::distance<T>),
        "Get the distance from a point to a line segment");
    m.def("distance", py::overload_cast<const Point2<T>&, const Segment2<T>&>(&dgal::distance<T>),
        "Get the distance from a point to a line segment");
    m.def("distance", [](const Quad2<T>& box, const Point2<T>& p){ return dgal::distance(box, p); },
        "Get the distance from a point to a box");
    m.def("distance", [](const Point2<T>& p, const Quad2<T>& box){ return dgal::distance(p, box); },
        "Get the distance from a point to a box");
    m.def("distance", [](const Quad2<T>& box, const Point2<T>& p){
            uint8_t idx; distance(box, p, idx); return idx;
        }, "Get the distance from a point to a box");
    m.def("intersect", py::overload_cast<const Line2<T>&, const Line2<T>&>(&dgal::intersect<T>),
        "Get the intersection point of two lines");
    m.def("intersect", [](const Quad2<T>& b1, const Quad2<T>& b2){ return dgal::intersect(b1, b2); },
        "Get the intersection polygon of two boxes");
    m.def("intersect_", [](const Quad2<T>& b1, const Quad2<T>& b2, const Algorithm alg){
            uint8_t xflags[8]; Poly2<T, 8> result;
            switch(alg)
            {
                default:
                case Algorithm::Default:
                    result = dgal::intersect(b1, b2, xflags);
                    break;
                case Algorithm::RotatingCaliper:
                    result = dgal::intersect(AlgorithmT::RotatingCaliper(), b1, b2, xflags);
                    break;
                case Algorithm::SutherlandHodgeman:
                    result = dgal::intersect(AlgorithmT::SutherlandHodgeman(), b1, b2, xflags);
                    break;
            }
            vector<uint8_t> xflags_v(xflags, xflags + result.nvertices);
            return make_tuple(result, xflags_v);
        }, "Get the intersection polygon of two boxes and return flags");
    m.def("intersect", py::overload_cast<const AABox2<T>&, const AABox2<T>&>(&dgal::intersect<T>),
        "Get the intersection box of two axis aligned boxes");
    m.def("merge", py::overload_cast<const AABox2<T>&, const AABox2<T>&>(&dgal::merge<T>),
        "Get bounding box of two axis aligned boxes");
    m.def("merge", [](const Quad2<T>& b1, const Quad2<T>& b2){ return dgal::merge(b1, b2); },
        "Get merged convex hull of two polygons");
    m.def("merge_", [](const Quad2<T>& b1, const Quad2<T>& b2){
            uint8_t mflags[8];
            auto result = dgal::intersect(b1, b2, mflags);
            vector<uint8_t> mflags_v(mflags, mflags + result.nvertices);
            return make_tuple(result, mflags_v);
        }, "Get merged convex hull of two polygons");
    m.def("max_distance", py::overload_cast<const Quad2<T>&, const Quad2<T>&>(&dgal::max_distance<T, 4, 4>),
        "Get the max distance between two polygons");
    m.def("max_distance", py::overload_cast<const AABox2<T>&, const AABox2<T>&>(&dgal::max_distance<T>),
        "Get the max distance between two polygons");
    m.def("iou", py::overload_cast<const AABox2<T>&, const AABox2<T>&>(&dgal::iou<T>),
        "Get the intersection over union of two axis aligned boxes");
    m.def("iou", py::overload_cast<const Quad2<T>&, const Quad2<T>&>(&dgal::iou<T, 4, 4>),
        "Get the intersection over union of two boxes");
    m.def("iou_", [](const Quad2<T>& b1, const Quad2<T>& b2){
            uint8_t xflags[8]; uint8_t nx;
            auto result = dgal::iou(b1, b2, nx, xflags);
            vector<uint8_t> xflags_v(xflags, xflags + nx);
            return make_tuple(result, xflags_v);
        }, "Get the intersection over union of two boxes and return flags");
    m.def("giou", py::overload_cast<const AABox2<T>&, const AABox2<T>&>(&dgal::giou<T>),
        "Get the generalized intersection over union of two axis aligned boxes");
    m.def("giou", py::overload_cast<const Quad2<T>&, const Quad2<T>&>(&dgal::giou<T, 4, 4>),
        "Get the generalized intersection over union of two boxes");
    m.def("giou_", [](const Quad2<T>& b1, const Quad2<T>& b2){
            uint8_t xflags[8], mflags[8]; uint8_t nx, nm;
            auto result = dgal::giou(b1, b2, nx, nm, xflags, mflags);
            vector<uint8_t> xflags_v(xflags, xflags + nx);
            vector<uint8_t> mflags_v(mflags, mflags + nm);
            return make_tuple(result, xflags_v, mflags_v);
        }, "Get the generalized intersection over union of two boxes and return flags");
    m.def("diou", py::overload_cast<const AABox2<T>&, const AABox2<T>&>(&dgal::diou<T>),
        "Get the distance intersection over union of two axis aligned boxes");
    m.def("diou", py::overload_cast<const Quad2<T>&, const Quad2<T>&>(&dgal::diou<T, 4, 4>),
        "Get the distance intersection over union of two boxes");
    m.def("diou_", [](const Quad2<T>& b1, const Quad2<T>& b2){
            uint8_t xflags[8]; uint8_t nx, dflag1, dflag2;
            auto result = dgal::diou(b1, b2, nx, dflag1, dflag2, xflags);
            vector<uint8_t> xflags_v(xflags, xflags + nx);
            return make_tuple(result, xflags_v, dflag1, dflag2);
        }, "Get the distance intersection over union of two boxes and return flags");

    // gradient functions from geometry_grad.hpp
    // gradient of constructors

    m.def("line2_from_pp_grad", &dgal::line2_from_pp_grad<T>, "Calculate gradient of line2_from_pp()");
    m.def("line2_from_xyxy_grad", &dgal::line2_from_pp_grad<T>, "Calculate gradient of line2_from_xyxy()");
    m.def("segment2_from_pp_grad", &dgal::segment2_from_pp_grad<T>, "Calculate gradient of segment2_from_pp_grad()");
    m.def("line2_from_segment2_grad", &dgal::line2_from_segment2_grad<T>, "Calculate gradient of line2_from_segment2_grad()");
    m.def("poly2_from_aabox2_grad", &dgal::poly2_from_aabox2_grad<T>, "Calculate gradient of poly2_from_aabox2()");
    m.def("poly2_from_xywhr_grad", [](const T& x, const T& y,
        const T& w, const T& h, const T& r, const Quad2<T>& grad){
            T gx = 0, gy = 0, gw = 0, gh = 0, gr = 0;
            poly2_from_xywhr_grad(x, y, w, h, r, grad, gx, gy, gw, gh, gr);
            return make_tuple(gx, gy, gw, gh, gr);
        }, "Calculate gradient of poly2_from_xywhr");
    m.def("aabox2_from_poly2_grad", &dgal::aabox2_from_poly2_grad<T, 4>, "Calculate gradient of aabox2_from_poly2_grad()");
    m.def("aabox2_from_poly2_grad", &dgal::aabox2_from_poly2_grad<T, 8>, "Calculate gradient of aabox2_from_poly2_grad()");
    m.def("intersect_grad", py::overload_cast<const Line2<T>&, const Line2<T>&, const Point2<T>&, Line2<T>&, Line2<T>&>(&dgal::intersect_grad<T>),
        "Calculate gradient of intersect");

    // gradient of functions

    m.def("distance_grad", py::overload_cast<const Point2<T>&, const Point2<T>&, const T&, Point2<T>&, Point2<T>&>(&dgal::distance_grad<T>), "Calculate gradient of distance()");
    m.def("distance_grad", py::overload_cast<const Line2<T>&, const Point2<T>&, const T&, Line2<T>&, Point2<T>&>(&dgal::distance_grad<T>), "Calculate gradient of distance()");
    m.def("distance_grad", py::overload_cast<const Segment2<T>&, const Point2<T>&, const T&, Segment2<T>&, Point2<T>&>(&dgal::distance_grad<T>), "Calculate gradient of distance()");
    m.def("distance_grad", [](const Quad2<T>& b, const Point2<T>& p, const T& grad, Quad2<T>& grad_b, Point2<T>& grad_p, const uint8_t& idx) {
        dgal::distance_grad(b, p, grad, grad_b, grad_p, idx);
    }, "Calculate the gradient of distance()");
    m.def("area_grad", py::overload_cast<const AABox2<T>&, const T&, AABox2<T>&>(&dgal::area_grad<T>), "Calculate gradient of area()");
    m.def("area_grad", py::overload_cast<const Quad2<T>&, const T&, Quad2<T>&>(&dgal::area_grad<T, 4>), "Calculate gradient of area()");
    m.def("area_grad", py::overload_cast<const Poly2<T, 8>&, const T&, Poly2<T, 8>&>(&dgal::area_grad<T, 8>), "Calculate gradient of area()");
    m.def("dimension_grad", py::overload_cast<const AABox2<T>&, const T&, AABox2<T>&>(&dgal::dimension_grad<T>), "Calculate gradient of dimension()");
    m.def("dimension_grad", py::overload_cast<const Quad2<T>&, const T&, const uint8_t&, const uint8_t&, Quad2<T>&>(&dgal::dimension_grad<T, 4>),
    "Calculate gradient of dimension()");
    m.def("dimension_grad", py::overload_cast<const Poly2<T, 8>&, const T&, const uint8_t&, const uint8_t&, Poly2<T, 8>&>(&dgal::dimension_grad<T, 8>),
    "Calculate gradient of dimension()");
    m.def("center_grad", py::overload_cast<const AABox2<T>&, const Point2<T>&, AABox2<T>&>(&dgal::center_grad<T>), "Calculate gradient of center()");
    m.def("center_grad", py::overload_cast<const Quad2<T>&, const Point2<T>&, Quad2<T>&>(&dgal::center_grad<T, 4>), "Calculate gradient of center()");
    m.def("center_grad", py::overload_cast<const Poly2<T, 8>&, const Point2<T>&, Poly2<T, 8>&>(&dgal::center_grad<T, 8>), "Calculate gradient of center()");
    m.def("centroid_grad", py::overload_cast<const AABox2<T>&, const Point2<T>&, AABox2<T>&>(&dgal::centroid_grad<T>), "Calculate gradient of centroid()");
    m.def("centroid_grad", py::overload_cast<const Quad2<T>&, const Point2<T>&, Quad2<T>&>(&dgal::centroid_grad<T, 4>), "Calculate gradient of centroid()");
    m.def("centroid_grad", py::overload_cast<const Poly2<T, 8>&, const Point2<T>&, Poly2<T, 8>&>(&dgal::centroid_grad<T, 8>), "Calculate gradient of centroid()");

    // gradient of operators

    m.def("intersect_grad", py::overload_cast<const Line2<T>&, const Line2<T>&, const Point2<T>&, Line2<T>&, Line2<T>&>(&dgal::intersect_grad<T>),
        "Calculate gradient of intersect()");
    m.def("intersect_grad", [](const Quad2<T>& b1, const Quad2<T>& b2, const Poly2<T, 8>& grad, const vector<uint8_t>& xflags){
            Quad2<T> grad_p1, grad_p2;
            grad_p1.zero(); grad_p2.zero();
            intersect_grad(b1, b2, grad, xflags.data(), grad_p1, grad_p2);
            return make_tuple(grad_p1, grad_p2);
        }, "Calculate gradient of intersect()");
    m.def("intersect_grad", py::overload_cast<const AABox2<T>&, const AABox2<T>&, const AABox2<T>&, AABox2<T>&, AABox2<T>&>(&dgal::intersect_grad<T>),
        "Calculate gradient of intersect()");
    m.def("merge_grad", [](const Quad2<T>& b1, const Quad2<T>& b2, const Poly2<T, 8>& grad, const vector<uint8_t>& mflags){
            Quad2<T> grad_p1, grad_p2;
            grad_p1.zero(); grad_p2.zero();
            merge_grad(b1, b2, grad, mflags.data(), grad_p1, grad_p2);
            return make_tuple(grad_p1, grad_p2);
        }, "Calculate gradient of merge()");
    m.def("merge_grad", py::overload_cast<const AABox2<T>&, const AABox2<T>&, const AABox2<T>&, AABox2<T>&, AABox2<T>&>(&dgal::merge_grad<T>),
        "Calculate gradient of merge()");
    m.def("iou_grad", [](const Quad2<T>& b1, const Quad2<T>& b2, const T grad, const vector<uint8_t>& xflags){
            Quad2<T> grad_p1, grad_p2;
            grad_p1.zero(); grad_p2.zero();
            iou_grad(b1, b2, grad, xflags.size(), xflags.data(), grad_p1, grad_p2);
            return make_tuple(grad_p1, grad_p2);
        }, "Calculate gradient of iou()");
    m.def("iou_grad", py::overload_cast<const AABox2<T>&, const AABox2<T>&, const T&, AABox2<T>&, AABox2<T>&>(&dgal::iou_grad<T>),
        "Calculate gradient of iou()");
    m.def("giou_grad", [](const Quad2<T>& b1, const Quad2<T>& b2, const T grad,
        const vector<uint8_t>& xflags, const vector<uint8_t>& mflags){
            Quad2<T> grad_p1, grad_p2;
            grad_p1.zero(); grad_p2.zero();
            giou_grad(b1, b2, grad, xflags.size(), mflags.size(), xflags.data(), mflags.data(), grad_p1, grad_p2);
            return make_tuple(grad_p1, grad_p2);
        }, "Calculate gradient of giou()");
    m.def("giou_grad", py::overload_cast<const AABox2<T>&, const AABox2<T>&, const T&, AABox2<T>&, AABox2<T>&>(&dgal::giou_grad<T>),
        "Calculate gradient of giou()");
    m.def("diou_grad", [](const Quad2<T>& b1, const Quad2<T>& b2, const T grad,
    const vector<uint8_t>& xflags, const uint8_t& dflag1, const uint8_t& dflag2){
            Quad2<T> grad_p1, grad_p2;
            grad_p1.zero(); grad_p2.zero();
            diou_grad(b1, b2, grad, xflags.size(), dflag1, dflag2, xflags.data(), grad_p1, grad_p2);
            return make_tuple(grad_p1, grad_p2);
        }, "Calculate gradient of diou()");
    m.def("diou_grad", py::overload_cast<const AABox2<T>&, const AABox2<T>&, const T&, AABox2<T>&, AABox2<T>&>(&dgal::diou_grad<T>),
        "Calculate gradient of diou()");
}