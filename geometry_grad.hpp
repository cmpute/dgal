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
 * This file contains implementations of gradient calculation of simple geometries suitable for using in GPU.
 * The gradients are stored in the same data structure as data passed forward.
 * 
 * Note:
 * - For polygon gradients, since the gradients are calculated additively, it's required to initialize the
 *   output gradient polygon objects to zeros (using .zero()).
 */

#ifndef DGAL_GEOMETRY_GRAD_HPP
#define DGAL_GEOMETRY_GRAD_HPP

#include "dgal/geometry.hpp"

namespace dgal
{

template <typename T> constexpr CUDA_CALLABLE_MEMBER inline
void _max_grad(const T &a, const T &b, const T &g, T &ga, T &gb)
{
    if (a > b) ga += g; else gb += g;
}
template <typename T> constexpr CUDA_CALLABLE_MEMBER inline
void _min_grad(const T &a, const T &b, const T &g, T &ga, T &gb)
{
    if (a < b) ga += g; else gb += g; 
}

///////////// gradient implementation of constructors //////////////

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void line2_from_xyxy_grad(const scalar_t &x1, const scalar_t &y1, const scalar_t &x2, const scalar_t &y2,
    const Line2<scalar_t> &grad, scalar_t &x1_grad, scalar_t &y1_grad, scalar_t &x2_grad, scalar_t &y2_grad)
{
    x1_grad +=  grad.b-y2*grad.c;
    x2_grad += -grad.b+y1*grad.c;
    y1_grad += -grad.a+x2*grad.c;
    y2_grad +=  grad.a-x1*grad.c;
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void line2_from_pp_grad(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2, const Line2<scalar_t> &grad,
    Point2<scalar_t> &grad_p1, Point2<scalar_t> &grad_p2)
{
    return line2_from_xyxy_grad(p1.x, p1.y, p2.x, p2.y, grad, grad_p1.x, grad_p1.y, grad_p2.x, grad_p2.y);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void segment2_from_pp_grad(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2, const Segment2<scalar_t> &grad,
    Point2<scalar_t> &grad_p1, Point2<scalar_t> &grad_p2)
{
    grad_p1.x += grad.x1;
    grad_p1.y += grad.y1;
    grad_p2.x += grad.x2;
    grad_p2.y += grad.y2;
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void line2_from_segment2_grad(const Segment2<scalar_t> &s, const Line2<scalar_t> &grad, Segment2<scalar_t> &grad_s)
{
    line2_from_xyxy_grad(s.x1, s.y1, s.x2, s.y2, grad, grad_s.x1, grad_s.y1, grad_s.x2, grad_s.y2);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void poly2_from_aabox2_grad(const AABox2<scalar_t> &a, const Poly2<scalar_t, 4> &grad,
    AABox2<scalar_t> &grad_a)
{
    assert(grad.nvertices == 4);
    grad_a.min_x += grad.vertices[0].x + grad.vertices[3].x;
    grad_a.max_x += grad.vertices[1].x + grad.vertices[2].x;
    grad_a.min_y += grad.vertices[0].y + grad.vertices[1].y;
    grad_a.max_y += grad.vertices[2].y + grad.vertices[3].y;
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
void aabox2_from_poly2_grad(const Poly2<scalar_t, MaxPoints> &p, const AABox2<scalar_t> &grad,
    Poly2<scalar_t, MaxPoints> &grad_p)
{
    scalar_t max_x = p.vertices[0].x, min_x = p.vertices[0].x;
    scalar_t max_y = p.vertices[0].y, min_y = p.vertices[0].y;
    uint8_t imax_x = 0, imin_x = 0, imax_y = 0, imin_y = 0;
    
    // find position for minmax values
    for (uint8_t i = 1; i < p.nvertices; i++)
    {
        if (p.vertices[i].x > max_x) { max_x = p.vertices[i].x; imax_x = i; }
        if (p.vertices[i].x < min_x) { min_x = p.vertices[i].x; imin_x = i; }
        if (p.vertices[i].y > max_y) { max_y = p.vertices[i].y; imax_y = i; }
        if (p.vertices[i].y < min_y) { min_y = p.vertices[i].y; imin_y = i; }
    }

    // pass the gradients
    grad_p.nvertices = p.nvertices;
    for (uint8_t i = 0; i < p.nvertices; i++)
    {
        if (i == imax_x) grad_p.vertices[i].x += grad.max_x;
        if (i == imin_x) grad_p.vertices[i].x += grad.min_x;
        if (i == imax_y) grad_p.vertices[i].y += grad.max_y;
        if (i == imin_y) grad_p.vertices[i].y += grad.min_y;
    }
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void poly2_from_xywhr_grad(const scalar_t& x, const scalar_t& y,
    const scalar_t& w, const scalar_t& h, const scalar_t& r, const Poly2<scalar_t, 4>& grad,
    scalar_t &grad_x, scalar_t &grad_y, scalar_t &grad_w, scalar_t &grad_h, scalar_t &grad_r)
{
    assert(grad.nvertices == 4);

    grad_x += grad.vertices[0].x + grad.vertices[1].x + grad.vertices[2].x + grad.vertices[3].x;
    grad_y += grad.vertices[0].y + grad.vertices[1].y + grad.vertices[2].y + grad.vertices[3].y;

    scalar_t dxsin_grad = -grad.vertices[0].y + grad.vertices[1].y + grad.vertices[2].y - grad.vertices[3].y;
    scalar_t dxcos_grad = -grad.vertices[0].x + grad.vertices[1].x + grad.vertices[2].x - grad.vertices[3].x;
    scalar_t dysin_grad =  grad.vertices[0].x + grad.vertices[1].x - grad.vertices[2].x - grad.vertices[3].x;
    scalar_t dycos_grad = -grad.vertices[0].y - grad.vertices[1].y + grad.vertices[2].y + grad.vertices[3].y;

    grad_w += (dxsin_grad*sin(r) + dxcos_grad*cos(r))/2;
    grad_h += (dysin_grad*sin(r) + dycos_grad*cos(r))/2;
    grad_r += (dxsin_grad*w*cos(r) - dxcos_grad*w*sin(r)
             + dysin_grad*h*cos(r) - dycos_grad*h*sin(r))/2;
}

///////////// gradient implementation of functions //////////////

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void distance_grad(const Point2<scalar_t> &p1, const Point2<scalar_t> &p2, const scalar_t &grad,
    Point2<scalar_t> &grad_p1, Point2<scalar_t> &grad_p2)
{
    scalar_t dx = p1.x - p2.x, dy = p1.y - p2.y, d = hypot(dx, dy);
    grad_p1.x += grad * dx/d;
    grad_p2.x -= grad * dx/d;
    grad_p1.y += grad * dy/d;
    grad_p2.y -= grad * dy/d;
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void distance_grad(const Line2<scalar_t> &l, const Point2<scalar_t> &p, const scalar_t &grad,
    Line2<scalar_t> &grad_l, Point2<scalar_t> &grad_p)
{
    scalar_t hab = hypot(l.a, l.b);
    scalar_t hab3 = hab * hab * hab;

    grad_p.x += grad * l.a / hab;
    grad_p.y += grad * l.b / hab;
    grad_l.a += grad * l.a * p.y * p.y / hab3;
    grad_l.b += grad * l.b * p.x * p.x / hab3;
    grad_l.c += grad * 1 / hab;
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void distance_grad(const Segment2<scalar_t> &s, const Point2<scalar_t> &p, const scalar_t &grad,
    Segment2<scalar_t> &grad_s, Point2<scalar_t> &grad_p)
{
    Line2<scalar_t> l = line2_from_segment2(s);
    scalar_t t = t_from_ppoint(l, p);
    scalar_t sign = l.a*p.x + l.b*p.y + l.c;

    if (t < t_from_pxy(l, s.x2, s.y2))
    {
        Point2<scalar_t> p2 {.x=s.x2, .y=s.y2};
        Point2<scalar_t> grad_p2;
        scalar_t d = distance(p, p2);
        distance_grad(p, p2, sign > 0 ? grad : -grad, grad_p, grad_p2);
        grad_s.x2 += grad_p2.x;
        grad_s.y2 += grad_p2.y;
    }
    else if(t > t_from_pxy(l, s.x1, s.y1))
    {
        Point2<scalar_t> p1 {.x=s.x1, .y=s.y1};
        Point2<scalar_t> grad_p1;
        scalar_t d = distance(p, p1);
        distance_grad(p, p1, sign > 0 ? grad : -grad, grad_p, grad_p1);
        grad_s.x1 += grad_p1.x;
        grad_s.y1 += grad_p1.y;
    }
    else
    {
        Line2<scalar_t> grad_l;
        distance_grad(l, p, grad, grad_l, grad_p);
        line2_from_segment2_grad(s, grad_l, grad_s);
    }
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
void distance_grad(const Poly2<scalar_t, MaxPoints> &poly, const Point2<scalar_t> &p, const scalar_t grad,
    Poly2<scalar_t, MaxPoints> &grad_poly, Point2<scalar_t> &grad_p, const uint8_t &idx)
{
    uint8_t nidx = _mod_inc(idx, poly.nvertices);
    Segment2<scalar_t> s = segment2_from_pp(poly.vertices[idx], poly.vertices[nidx]);
    Segment2<scalar_t> grad_s;
    distance_grad(s, p, -grad, grad_s, grad_p);
    segment2_from_pp_grad(poly.vertices[idx], poly.vertices[nidx], grad_s, grad_poly.vertices[idx], grad_poly.vertices[nidx]);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void area_grad(const AABox2<scalar_t> &a, const scalar_t &grad, AABox2<scalar_t> &grad_a)
{
    scalar_t lx = a.max_x - a.min_x;
    scalar_t ly = a.max_y - a.min_y;
    grad_a.max_x += grad * ly;
    grad_a.min_x -= grad * ly;
    grad_a.max_y += grad * lx;
    grad_a.min_y -= grad * lx;
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
void area_grad(const Poly2<scalar_t, MaxPoints> &p, const scalar_t &grad, Poly2<scalar_t, MaxPoints> &grad_p)
{
    if (p.nvertices <= 2) return;
    grad_p.nvertices = p.nvertices;

    scalar_t hgrad = grad / 2;

    // deal with head and tail vertex
    grad_p.vertices[0].x             -= hgrad * p.vertices[p.nvertices-1].y;
    grad_p.vertices[0].y             += hgrad * p.vertices[p.nvertices-1].x;
    grad_p.vertices[p.nvertices-1].x += hgrad * p.vertices[0].y;
    grad_p.vertices[p.nvertices-1].y -= hgrad * p.vertices[0].x;

    // process other vertices
    for (uint8_t i = 1; i < p.nvertices; i++)
    {
        grad_p.vertices[i].x   -= hgrad * p.vertices[i-1].y;
        grad_p.vertices[i].y   += hgrad * p.vertices[i-1].x;
        grad_p.vertices[i-1].x += hgrad * p.vertices[i].y;
        grad_p.vertices[i-1].y -= hgrad * p.vertices[i].x;
    }
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void dimension_grad(const AABox2<scalar_t> &a, const scalar_t &grad, AABox2<scalar_t> &grad_a)
{
    // basically equivalent to distance_grad of (max_x, max_y) and (min_x, min_y)
    scalar_t dx = a.max_x - a.min_x, dy = a.max_y - a.min_y, d = hypot(dx, dy);
    grad_a.max_x += grad * dx / d;
    grad_a.min_x -= grad * dx / d;
    grad_a.max_y += grad * dy / d;
    grad_a.min_y -= grad * dy / d;
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
void dimension_grad(const Poly2<scalar_t, MaxPoints> &p, const scalar_t &grad,
    const uint8_t &flag1, const uint8_t &flag2, Poly2<scalar_t, MaxPoints> &grad_p)
{
    distance_grad(p.vertices[flag1], p.vertices[flag2], grad, grad_p.vertices[flag1], grad_p.vertices[flag2]);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void center_grad(const AABox2<scalar_t> &a, const Point2<scalar_t> &grad, AABox2<scalar_t> &grad_a)
{
    grad_a.max_x += grad.x / 2;
    grad_a.min_x += grad.x / 2;
    grad_a.max_y += grad.y / 2;
    grad_a.min_y += grad.y / 2;
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
void center_grad(const Poly2<scalar_t, MaxPoints> &p, const Point2<scalar_t> &grad, Poly2<scalar_t, MaxPoints> &grad_p)
{
    AABox2<scalar_t> a = aabox2_from_poly2(p), grad_a;
    center_grad(a, grad, grad_a);
    aabox2_from_poly2_grad(p, grad_a, grad_p);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void centroid_grad(const AABox2<scalar_t> &a, const Point2<scalar_t> &grad, AABox2<scalar_t> &grad_a)
{
    return center_grad(a, grad, grad_a);
}

template <typename scalar_t, uint8_t MaxPoints> CUDA_CALLABLE_MEMBER inline
void centroid_grad(const Poly2<scalar_t, MaxPoints> &p, const Point2<scalar_t> &grad, Poly2<scalar_t, MaxPoints> &grad_p)
{
    grad_p.nvertices = p.nvertices;
    for (uint8_t i = 0; i < p.nvertices; i++)
    {
        grad_p.vertices[i].x += grad.x / p.nvertices;
        grad_p.vertices[i].y += grad.y / p.nvertices;
    }
}

///////////// gradient implementation of operators //////////////

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void intersect_grad(const Line2<scalar_t> &l1, const Line2<scalar_t> &l2, const Point2<scalar_t> &grad,
    Line2<scalar_t> &grad_l1, Line2<scalar_t> &grad_l2)
{
    scalar_t wab = l1.a*l2.b - l2.a*l1.b;
    scalar_t wbc = l1.b*l2.c - l2.b*l1.c;
    scalar_t wca = l1.c*l2.a - l2.c*l1.a;

    scalar_t g_wbc = grad.x/wab;
    scalar_t g_wca = grad.y/wab;
    scalar_t g_wab = -(wbc*g_wbc + wca*g_wca)/wab;

    grad_l1.a += -l2.c*g_wca + l2.b*g_wab;
    grad_l1.b +=  l2.c*g_wbc - l2.a*g_wab;
    grad_l1.c +=  l2.a*g_wca - l2.b*g_wbc;
    grad_l2.a +=  l1.c*g_wca - l1.b*g_wab;
    grad_l2.b += -l1.c*g_wbc + l1.a*g_wab;
    grad_l2.c +=  l1.b*g_wbc - l1.a*g_wca;
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
void intersect_grad(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const Poly2<scalar_t, MaxPoints1 + MaxPoints2> grad, const CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2],
    Poly2<scalar_t, MaxPoints1> &grad_p1, Poly2<scalar_t, MaxPoints2> &grad_p2
) {
    // initialize output
    grad_p1.nvertices = p1.nvertices;
    grad_p2.nvertices = p2.nvertices;

    for (uint8_t i = 0; i < grad.nvertices; i++)
    {
        uint8_t iprev = _mod_dec(i, grad.nvertices);
        if ((xflags[i] & 1) == (xflags[iprev] & 1)) // the intersection vertex is from one of the polygons
        {
            if (xflags[i] & 1)
                grad_p1.vertices[xflags[i] >> 1] += grad.vertices[i];
            else
                grad_p2.vertices[xflags[i] >> 1] += grad.vertices[i];
        }
        else // the intersection vertex is defined by both polygons
        {
            Line2<scalar_t> edge_next, edge_prev, grad_edge_next, grad_edge_prev;
            if (xflags[i] & 1) // next edge is from p1 and previous edge is from p2
            {
                uint8_t epni = xflags[i    ] >> 1, epnj = _mod_inc(epni, p1.nvertices);
                uint8_t eppi = xflags[iprev] >> 1, eppj = _mod_inc(eppi, p2.nvertices);
                edge_next = line2_from_pp(p1.vertices[epni], p1.vertices[epnj]);
                edge_prev = line2_from_pp(p2.vertices[eppi], p2.vertices[eppj]);
                intersect_grad(edge_next, edge_prev, grad.vertices[i], grad_edge_next, grad_edge_prev);
                line2_from_pp_grad(p1.vertices[epni], p1.vertices[epnj], grad_edge_next, grad_p1.vertices[epni], grad_p1.vertices[epnj]);
                line2_from_pp_grad(p2.vertices[eppi], p2.vertices[eppj], grad_edge_prev, grad_p2.vertices[eppi], grad_p2.vertices[eppj]);
            }
            else // next edge is from p2 and previous edge is from p1
            {
                uint8_t epni = xflags[i    ] >> 1, epnj = _mod_inc(epni, p2.nvertices);
                uint8_t eppi = xflags[iprev] >> 1, eppj = _mod_inc(eppi, p1.nvertices);
                edge_next = line2_from_pp(p2.vertices[epni], p2.vertices[epnj]);
                edge_prev = line2_from_pp(p1.vertices[eppi], p1.vertices[eppj]);
                intersect_grad(edge_next, edge_prev, grad.vertices[i], grad_edge_next, grad_edge_prev);
                line2_from_pp_grad(p2.vertices[epni], p2.vertices[epnj], grad_edge_next, grad_p2.vertices[epni], grad_p2.vertices[epnj]);
                line2_from_pp_grad(p1.vertices[eppi], p1.vertices[eppj], grad_edge_prev, grad_p1.vertices[eppi], grad_p1.vertices[eppj]);
            }
        }
    }
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void intersect_grad(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2, const AABox2<scalar_t> &grad,
    AABox2<scalar_t> &grad_a1, AABox2<scalar_t> &grad_a2)
{
    _max_grad(a1.min_x, a2.min_x, grad.min_x, grad_a1.min_x, grad_a2.min_x);
    _min_grad(a1.max_x, a2.max_x, grad.max_x, grad_a1.max_x, grad_a2.max_x);
    _max_grad(a1.min_y, a2.min_y, grad.min_y, grad_a1.min_y, grad_a2.min_y);
    _min_grad(a1.max_y, a2.max_y, grad.max_y, grad_a1.max_y, grad_a2.max_y);
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
void merge_grad(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const Poly2<scalar_t, MaxPoints1 + MaxPoints2> &grad,
    const CUDA_RESTRICT uint8_t mflags[MaxPoints1 + MaxPoints2],
    Poly2<scalar_t, MaxPoints1> &grad_p1, Poly2<scalar_t, MaxPoints2> &grad_p2
) {
    grad_p1.nvertices = p1.nvertices;
    grad_p2.nvertices = p2.nvertices;

    for (uint8_t i = 0; i < grad.nvertices; i++)
    {
        if (mflags[i] & 1)
            grad_p1.vertices[mflags[i] >> 1] += grad.vertices[i];
        else
            grad_p2.vertices[mflags[i] >> 1] += grad.vertices[i];
    }
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void merge_grad(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2, const AABox2<scalar_t> &grad,
    AABox2<scalar_t> &grad_a1, AABox2<scalar_t> &grad_a2)
{
    _min_grad(a1.min_x, a2.min_x, grad.min_x, grad_a1.min_x, grad_a2.min_x);
    _max_grad(a1.max_x, a2.max_x, grad.max_x, grad_a1.max_x, grad_a2.max_x);
    _min_grad(a1.min_y, a2.min_y, grad.min_y, grad_a1.min_y, grad_a2.min_y);
    _max_grad(a1.max_y, a2.max_y, grad.max_y, grad_a1.max_y, grad_a2.max_y);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void iou_grad(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2, const scalar_t &grad,
    AABox2<scalar_t> &grad_a1, AABox2<scalar_t> &grad_a2)
{
    AABox2<scalar_t> ai = intersect(a1, a2);
    scalar_t area_i = area(ai);
    scalar_t area_u = area(a1) + area(a2) - area_i;

    scalar_t gi =  grad / area_u;
    scalar_t gu = -gi * area_i / area_u; // i.e. -grad * area_i / area_u^2
    gi -= gu;
    
    AABox2<scalar_t> grad_ai;
    area_grad(a1, gu, grad_a1);
    area_grad(a2, gu, grad_a2);
    area_grad(ai, gi, grad_ai);
    intersect_grad(a1, a2, grad_ai, grad_a1, grad_a2);
}

// construct intersection of two polygon from saved flags
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> _construct_intersection(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2], const uint8_t &nx
) {
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> pi; pi.nvertices = nx;
    Line2<scalar_t> edge_next, edge_prev;
    for (uint8_t i = 0; i < nx; i++)
    {
        uint8_t iprev = _mod_dec(i, nx);
        if ((xflags[i] & 1) == (xflags[iprev] & 1)) // the intersection vertex is from one of the polygons
        {
            if (xflags[i] & 1)
                pi.vertices[i] = p1.vertices[xflags[i] >> 1];
            else
                pi.vertices[i] = p2.vertices[xflags[i] >> 1];
        }
        else
        {
            if (xflags[i] & 1) // next edge is from p1 and previous edge is from p2
            {
                uint8_t epni = xflags[i    ] >> 1, epnj = _mod_inc(epni, p1.nvertices);
                uint8_t eppi = xflags[iprev] >> 1, eppj = _mod_inc(eppi, p2.nvertices);
                edge_next = line2_from_pp(p1.vertices[epni], p1.vertices[epnj]);
                edge_prev = line2_from_pp(p2.vertices[eppi], p2.vertices[eppj]);
            }
            else // next edge is from p2 and previous edge is from p1
            {
                uint8_t epni = xflags[i    ] >> 1, epnj = _mod_inc(epni, p2.nvertices);
                uint8_t eppi = xflags[iprev] >> 1, eppj = _mod_inc(eppi, p1.nvertices);
                edge_next = line2_from_pp(p2.vertices[epni], p2.vertices[epnj]);
                edge_prev = line2_from_pp(p1.vertices[eppi], p1.vertices[eppj]);
            }
            pi.vertices[i] = intersect(edge_prev, edge_next);
        }
    }
    return pi;
}

// construct merged hull of two polygon from saved flags
template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
Poly2<scalar_t, MaxPoints1 + MaxPoints2> _construct_merged_hull(
    const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const CUDA_RESTRICT uint8_t mflags[MaxPoints1 + MaxPoints2], const uint8_t &nm
) {
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> result;
    result.nvertices = nm;
    for (uint8_t i = 0; i < nm; i++)
        if (mflags[i] & 1)
            result.vertices[i] = p1.vertices[mflags[i] >> 1];
        else
            result.vertices[i] = p2.vertices[mflags[i] >> 1];
    return result;
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
void iou_grad(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const scalar_t &grad, const uint8_t &nx, const CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2],
    Poly2<scalar_t, MaxPoints1> &grad_p1, Poly2<scalar_t, MaxPoints2> &grad_p2
) {
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> pi = _construct_intersection(p1, p2, xflags, nx);
    scalar_t area_i = area(pi);
    scalar_t area_u = area(p1) + area(p2) - area_i;

    scalar_t gi =  grad / area_u;
    scalar_t gu = -gi * area_i / area_u; // i.e. -grad * area_i / area_u^2
    gi -= gu;
    
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> grad_pi; grad_pi.zero();
    area_grad(p1, gu, grad_p1);
    area_grad(p2, gu, grad_p2);
    area_grad(pi, gi, grad_pi);
    intersect_grad(p1, p2, grad_pi, xflags, grad_p1, grad_p2);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void giou_grad(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2, const scalar_t &grad,
    AABox2<scalar_t> &grad_a1, AABox2<scalar_t> &grad_a2)
{
    AABox2<scalar_t> ai = intersect(a1, a2), am = merge(a1, a2);
    scalar_t area_i = area(ai);
    scalar_t area_m = area(am);
    scalar_t area_u = area(a1) + area(a2) - area_i;

    scalar_t gi = grad / area_u;
    scalar_t gu = grad * (1 / area_m - area_i / (area_u * area_u));
    gi -= gu;
    scalar_t gm = grad * (-area_u / (area_m * area_m));
    
    AABox2<scalar_t> grad_ai, grad_am;
    area_grad(a1, gu, grad_a1);
    area_grad(a2, gu, grad_a2);
    area_grad(ai, gi, grad_ai);
    area_grad(am, gm, grad_am);
    intersect_grad(a1, a2, grad_ai, grad_a1, grad_a2);
    merge_grad(a1, a2, grad_am, grad_a1, grad_a2);
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
void giou_grad(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2,
    const scalar_t &grad, const uint8_t &nx, const uint8_t &nm,
    const CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2],
    const CUDA_RESTRICT uint8_t mflags[MaxPoints1 + MaxPoints2],
    Poly2<scalar_t, MaxPoints1> &grad_p1, Poly2<scalar_t, MaxPoints2> &grad_p2
) {
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> pi = _construct_intersection(p1, p2, xflags, nx);
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> pm = _construct_merged_hull(p1, p2, mflags, nm);
    scalar_t area_i = area(pi);
    scalar_t area_m = area(pm);
    scalar_t area_u = area(p1) + area(p2) - area_i;

    scalar_t gi = grad / area_u;
    scalar_t gu = grad * (1 / area_m - area_i / (area_u * area_u));
    gi -= gu;
    scalar_t gm = grad * (-area_u / (area_m * area_m));
    
    Poly2<scalar_t, MaxPoints1 + MaxPoints2> grad_pi, grad_pm;
    grad_pi.zero(); grad_pm.zero();
    area_grad(p1, gu, grad_p1);
    area_grad(p2, gu, grad_p2);
    area_grad(pi, gi, grad_pi);
    area_grad(pm, gm, grad_pm);
    intersect_grad(p1, p2, grad_pi, xflags, grad_p1, grad_p2);
    merge_grad(p1, p2, grad_pm, mflags, grad_p1, grad_p2);
}

template <typename scalar_t> CUDA_CALLABLE_MEMBER inline
void diou_grad(const AABox2<scalar_t> &a1, const AABox2<scalar_t> &a2, const scalar_t &grad,
    AABox2<scalar_t> &grad_a1, AABox2<scalar_t> &grad_a2)
{
    Point2<scalar_t> c1 = centroid(a1), c2 = centroid(a2);
    scalar_t cd = distance(c1, c2);
    AABox2<scalar_t> am = merge(a1, a2);
    scalar_t maxd = dimension(am);

    scalar_t grad_iou = grad;
    scalar_t grad_cd = -grad*2*cd/(maxd*maxd);
    scalar_t grad_maxd = grad*2*(cd*cd)/(maxd*maxd*maxd);

    iou_grad(a1, a2, grad_iou, grad_a1, grad_a2);
    Point2<scalar_t> grad_c1, grad_c2;
    AABox2<scalar_t> grad_am;
    distance_grad(c1, c2, grad_cd, grad_c1, grad_c2);
    centroid_grad(a1, grad_c1, grad_a1);
    centroid_grad(a2, grad_c2, grad_a2);
    dimension_grad(am, grad_maxd, grad_am);
    merge_grad(a1, a2, grad_am, grad_a1, grad_a2);
}

template <typename scalar_t, uint8_t MaxPoints1, uint8_t MaxPoints2> CUDA_CALLABLE_MEMBER inline
void diou_grad(const Poly2<scalar_t, MaxPoints1> &p1, const Poly2<scalar_t, MaxPoints2> &p2, const scalar_t &grad,
    const uint8_t &nx, const uint8_t &dflag1, const uint8_t &dflag2, const CUDA_RESTRICT uint8_t xflags[MaxPoints1 + MaxPoints2],
    Poly2<scalar_t, MaxPoints1> &grad_p1, Poly2<scalar_t, MaxPoints2> &grad_p2)
{
    Point2<scalar_t> c1 = centroid(p1), c2 = centroid(p2);
    scalar_t cd = distance(c1, c2);
    const Point2<scalar_t> &v1 = (dflag1 & 1) ? p1.vertices[dflag1 >> 1] : p2.vertices[dflag1 >> 1];
    const Point2<scalar_t> &v2 = (dflag2 & 1) ? p1.vertices[dflag2 >> 1] : p2.vertices[dflag2 >> 1];
    scalar_t maxd = distance(v1, v2);

    scalar_t grad_iou = grad;
    scalar_t grad_cd = -grad*2*cd/(maxd*maxd);
    scalar_t grad_maxd = grad*2*(cd*cd)/(maxd*maxd*maxd);

    iou_grad(p1, p2, grad_iou, nx, xflags, grad_p1, grad_p2);
    Point2<scalar_t> grad_c1, grad_c2;
    distance_grad(c1, c2, grad_cd, grad_c1, grad_c2);
    centroid_grad(p1, grad_c1, grad_p1);
    centroid_grad(p2, grad_c2, grad_p2);
    Point2<scalar_t> &grad_v1 = (dflag1 & 1) ? grad_p1.vertices[dflag1 >> 1] : grad_p2.vertices[dflag1 >> 1];
    Point2<scalar_t> &grad_v2 = (dflag2 & 1) ? grad_p1.vertices[dflag2 >> 1] : grad_p2.vertices[dflag2 >> 1];
    distance_grad(v1, v2, grad_maxd, grad_v1, grad_v2);
}

} // namespace dgal

#endif // DGAL_GEOMETRY_GRAD_HPP
