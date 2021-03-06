/*
 * Copyright (c) 2016-2017 lymastee, All rights reserved.
 * Contact: lymastee@hotmail.com
 *
 * This file is part of the gslib project.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <gslib/error.h>
#include <pink/raster.h>
#include <pink/utility.h>
#include <pink/clip.h>

__pink_begin__

static int get_quad_step(const vec2& p1, const vec2& p2, const vec2& p3)
{
    vec2 d;
    float d1, d2;
    d1 = d.sub(p2, p1).length();
    d2 = d.sub(p3, p2).length();
    return (int)sqrt((double)round(d1 + d2));
}

static int get_cubic_step(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4)
{
    vec2 d;
    float d1, d2, d3;
    d1 = d.sub(p2, p1).length();
    d2 = d.sub(p3, p2).length();
    d3 = d.sub(p4, p3).length();
    return (int)sqrt((double)round(d1 + d2 + d3));
}

void painter_path::quad_to_node::interpolate(painter_linestrip& c, const node* last) const
{
    const vec2& lastp = last->get_point();
    const quad_to_node* n = static_cast<const quad_to_node*>(this);
    const vec2& c1 = n->get_control();
    int cs = get_quad_step(lastp, c1, _pt);
    if(int ecs = cs - 1)
        quadratic_interpolate(c.expand(ecs) - 1, lastp, c1, _pt, ecs + 1);
}

void painter_path::cubic_to_node::interpolate(painter_linestrip& c, const node* last) const
{
    const vec2& lastp = last->get_point();
    const cubic_to_node* n = static_cast<const cubic_to_node*>(this);
    const vec2& c1 = n->get_control1();
    const vec2& c2 = n->get_control2();
    int cs = get_cubic_step(lastp, c1, c2, _pt);
    if(int ecs = cs - 1)
        cubic_interpolate(c.expand(ecs) - 1, lastp, c1, c2, _pt, ecs + 1);
}

void painter_path::resize(int len)
{
    int total = size();
    if(len >= total)
        return;
    for(int i = len; i < total; i ++) {
        auto* n = get_node(i);
        gs_del(node, n);
    }
    _nodelist.resize(len);
}

void painter_path::destroy()
{
    std::for_each(_nodelist.begin(), _nodelist.end(), [](node* p) { gs_del(node, p); });
    _nodelist.clear();
}

void painter_path::duplicate(const painter_path& path)
{
    painter_path cast;
    attach(cast);
    std::for_each(path.begin(), path.end(), [this](const node* n) {
        switch(n->get_tag())
        {
        case pt_moveto:
            move_to(n->get_point());
            break;
        case pt_lineto:
            line_to(n->get_point());
            break;
        case pt_quadto:
            quad_to(
                static_cast<const quad_to_node*>(n)->get_control(), 
                n->get_point()
                );
            break;
        case pt_cubicto:
            cubic_to(
                static_cast<const cubic_to_node*>(n)->get_control1(),
                static_cast<const cubic_to_node*>(n)->get_control2(),
                n->get_point()
                );
            break;
        }
    });
}

void painter_path::attach(painter_path& path)
{
    _nodelist.swap(path._nodelist);
}

void painter_path::close_path()
{
    if(_nodelist.empty());
    else if(_nodelist.size() == 1)
        line_to(_nodelist.front()->get_point());
    else {
        const vec2& p1 = _nodelist.front()->get_point();
        const vec2& p2 = _nodelist.back()->get_point();
        if(p1 != p2)
            line_to(p1);
    }
}

void painter_path::close_sub_path()
{
    if(_nodelist.empty());
    else if(_nodelist.size() == 1)
        line_to(_nodelist.front()->get_point());
    else {
        int cap = (int)_nodelist.size();
        node* back = _nodelist.back();
        for(int i = cap - 1; i >= 0; i --) {
            node* n = _nodelist.at(i);
            if(n->get_tag() == pt_moveto) {
                if(back->get_point() != n->get_point())
                    line_to(n->get_point());
                return;
            }
        }
        node* first = _nodelist.front();
        if(first->get_point() != back->get_point())
            line_to(first->get_point());
    }
}

/*
 * A improper way we used to describe the quad to node into cubic to forms, 
 * but now we prefer to keep them
void path::quad_to(const vec2& p1, const vec2& p2)
{
    const vec2& p0 = _nodelist.back()->get_point();
    if(p0 == p1 && p1 == p2)
        return;
    vec2 c1((p0.x + 2.f * p1.x) / 3.f, (p0.y + 2.f * p1.y) / 3.f);
    vec2 c2((p2.x + 2.f * p1.x) / 3.f, (p2.y + 2.f * p1.y) / 3.f);
    cubic_to(c1, c2, p2);
}
 */

static void transform_quarter_circle(painter_path& pa, const vec2& p1, const vec2& p2, const vec2& c, bool inv)
{
    static const float ratio = 5.f / 9;
    vec2 p, cc, m, n, c1, c2;
    vec2sub(&m, &p1, &c);
    vec2sub(&n, &p2, &c);
    vec2add(&p, &m, &n);
    vec2normalize(&p, &p);
    vec2scale(&p, &p, vec2length(&m));
    vec2add(&cc, &p2, &m);
    vec2scale(&c1, vec2sub(&c1, &cc, &p1), ratio);
    vec2scale(&c2, vec2sub(&c2, &cc, &p2), ratio);
    vec2add(&c1, &p1, &c1);
    vec2add(&c2, &p2, &c2);
    inv ? pa.cubic_to(c2, c1, p1) :
        pa.cubic_to(c1, c2, p2);
}

static void transform_half_circle(painter_path& pa, const vec2& p1, const vec2& p2, const vec2& p3, bool inv)
{
    vec2 c;
    vec2scale(&c, vec2add(&c, &p1, &p3), 0.5f);
    if(!inv) {
        transform_quarter_circle(pa, p1, p2, c, false);
        transform_quarter_circle(pa, p2, p3, c, false);
    }
    else {
        transform_quarter_circle(pa, p3, p2, c, false);
        transform_quarter_circle(pa, p2, p1, c, false);
    }
}

void painter_path::arc_to(const vec2& p1, const vec2& p2, float r)
{
    vec2 vd;
    float d = vec2length(vec2sub(&vd, &p2, &p1));
    float ar = abs(r);
    float cmp = fuzz_cmp(d, 2.f * ar, 0.1f);
    if(cmp < 0) {
        assert(!"invalid radius.");
        return;
    }
    const vec2& p0 = _nodelist.back()->get_point();
    if(cmp == 0) {
        vec2 p, v;
        vec2scale(&p, vec2add(&p, &p1, &p2), 0.5f);
        vec2sub(&v, &p1, &p);
        v = r > 0 ? vec2(v.y, -v.x) : vec2(-v.y, v.x);
        vec2add(&p, &p, &v);
        transform_half_circle(*this, p1, p, p2, p0 != p1);
        return;
    }
    // TODO
}

void painter_path::arc_to(const vec2& pt, float r)
{
    const vec2& p0 = _nodelist.back()->get_point();
    return arc_to(p0, pt, r);
}

void painter_path::rarc_to(const vec2& pt, float r)
{
    const vec2& p0 = _nodelist.back()->get_point();
    return arc_to(pt, p0, r);
}

void painter_path::transform(const mat3& m)
{
    int cap = size();
    for(int i = 0; i < cap; i ++) {
        auto* node = get_node(i);
        assert(node);
        switch(node->get_tag())
        {
        case pt_moveto:
        case pt_lineto:
            {
                vec2 p;
                const vec2& q = node->get_point();
                node->set_point(p.transformcoord(q, m));
                break;
            }
        case pt_quadto:
            {
                vec2 p;
                auto* n = static_cast<quad_to_node*>(node);
                const vec2& q0 = n->get_control();
                const vec2& q1 = n->get_point();
                n->set_control(p.transformcoord(q0, m));
                n->set_point(p.transformcoord(q1, m));
                break;
            }
        case pt_cubicto:
            {
                vec2 p;
                auto* n = static_cast<cubic_to_node*>(node);
                const vec2& q0 = n->get_control1();
                const vec2& q1 = n->get_control2();
                const vec2& q2 = n->get_point();
                n->set_control1(p.transformcoord(q0, m));
                n->set_control2(p.transformcoord(q1, m));
                n->set_point(p.transformcoord(q2, m));
                break;
            }
        default:
            assert(!"unexpected.");
            break;
        }
    }
}

void painter_path::get_linestrips(linestrips& c) const
{
    painter_linestrip* pc = 0;
    const node* last = 0;
    for(const_iterator i = _nodelist.begin(); i != _nodelist.end(); ++ i) {
        const node* n = *i;
        if(n->get_tag() == pt_moveto) {
            c.push_back(painter_linestrip());
            if(pc != 0)
                pc->finish();
            pc = &c.back();
            n->interpolate(*pc, 0);
        }
        else {
            assert(pc && last);
            n->interpolate(*pc, last);
        }
        last = n;
    }
    if(pc)  pc->finish();
}

int painter_path::get_control_contour(painter_linestrip& ls, int start) const
{
    assert(!ls.get_size());
    assert(start < size());
    auto* n = get_node(start);
    assert(n && n->get_tag() == pt_moveto);
    ls.add_point(n->get_point());
    int i = start + 1, end = size();
    for(; i < end; i ++) {
        auto* n = get_node(i);
        assert(n);
        auto t = n->get_tag();
        if(t == pt_moveto)
            break;
        switch(t)
        {
        case pt_lineto:
            ls.add_point(n->get_point());
            break;
        case pt_quadto:
            {
                auto* p = static_cast<const quad_to_node*>(n);
                ls.add_point(p->get_control());
                ls.add_point(p->get_point());
                break;
            }
        case pt_cubicto:
            {
                auto* p = static_cast<const cubic_to_node*>(n);
                ls.add_point(p->get_control1());
                ls.add_point(p->get_control2());
                ls.add_point(p->get_point());
                break;
            }
        }
    }
    int s = ls.get_size();
    assert(s > 1);
    auto& p1 = ls.get_point(0);
    auto& p2 = ls.get_point(s - 1);
    if(p1 == p2) {
        ls.expand_to(s - 1);
        ls.set_closed(true);
    }
    return i;
}

int painter_path::get_sub_path(painter_path& sp, int start) const
{
    assert(sp.empty());
    assert(start < size());
    auto* n = get_node(start);
    assert(n && n->get_tag() == pt_moveto);
    sp.move_to(n->get_point());
    int i = start + 1, end = size();
    for(; i < end; i ++) {
        auto* n = get_node(i);
        assert(n);
        auto t = n->get_tag();
        if(t == pt_moveto)
            break;
        switch(t)
        {
        case pt_lineto:
            sp.line_to(n->get_point());
            break;
        case pt_quadto:
            {
                auto* p = static_cast<const quad_to_node*>(n);
                sp.quad_to(p->get_control(), p->get_point());
                break;
            }
        case pt_cubicto:
            {
                auto* p = static_cast<const cubic_to_node*>(n);
                sp.cubic_to(p->get_control1(), p->get_control2(), p->get_point());
                break;
            }
        }
    }
    return i;
}

bool painter_path::is_clock_wise() const
{
    painter_linestrip ls;
    int next = get_control_contour(ls, 0);
    assert(next == size() &&
        "only path without sub paths could call this function."
        );
    return ls.is_clock_wise();
}

bool painter_path::is_convex() const
{
    painter_linestrip ls;
    int next = get_control_contour(ls, 0);
    assert(next == size() &&
        "only path without sub paths could call this function."
        );
    return ls.is_convex();
}

/*
 * The polygon clip problem was a really complex one.
 * Notice that this method should only applied with the situation that you are not 
 * sure if a path was complex one(IE, self intersected). If a path was already a simple one,
 * you needn't use this method.
 */

void painter_path::simplify(painter_path& path) const
{
    clip_polygons polys;
    clip_result result;
    painter_path tmp;
    clip_create_polygons(polys, *this);
    clip_exclude(result, polys);
    clip_compile_path(tmp, result);
    painter_helper::transform(path, tmp,
        painter_helper::merge_straight_line | painter_helper::reduce_short_line | painter_helper::reduce_straight_curve
        );
}

void painter_path::reverse()
{
    painter_path rp;
    int i = 0;
    for(;;) {
        painter_path subpath;
        i = get_sub_path(subpath, i);
        if(subpath.size() > 1) {
            int j = subpath.size() - 1;
            int k = j - 1;
            auto* n = subpath.get_node(j);
            rp.move_to(n->get_point());
            for(;;) {
                auto* cur = subpath.get_node(j);
                auto* prev = subpath.get_node(k);
                switch(cur->get_tag())
                {
                case pt_lineto:
                    rp.line_to(prev->get_point());
                    break;
                case pt_quadto:
                    rp.quad_to(static_cast<quad_to_node*>(cur)->get_control(),
                        prev->get_point()
                        );
                    break;
                case pt_cubicto:
                    rp.cubic_to(static_cast<cubic_to_node*>(cur)->get_control2(),
                        static_cast<cubic_to_node*>(cur)->get_control1(),
                        prev->get_point()
                        );
                    break;
                }
                j = k --;
                if(k < 0)
                    break;
            }
        }
        if(i >= size())
            break;
    }
    _nodelist.swap(rp._nodelist);
}

void painter_path::tracing() const
{
#if defined (DEBUG) || defined (_DEBUG)
    trace(_t("@!\n"));
    std::for_each(_nodelist.begin(), _nodelist.end(), [this](const node* n) {
        assert(n);
        switch(n->get_tag())
        {
        case pt_moveto:
            {
                const auto* p = static_cast<const move_to_node*>(n);
                const vec2& p1 = p->get_point();
                trace(_t("@moveTo %f, %f;\n"), p1.x, p1.y);
                break;
            }
        case pt_lineto:
            {
                const auto* p = static_cast<const line_to_node*>(n);
                const vec2& p1 = p->get_point();
                trace(_t("@lineTo %f, %f;\n"), p1.x, p1.y);
                break;
            }
        case pt_quadto:
            {
                const auto* p = static_cast<const quad_to_node*>(n);
                const vec2& p1 = p->get_control();
                const vec2& p2 = p->get_point();
                trace(_t("@quadraticTo %f, %f, %f, %f;\n"), p1.x, p1.y, p2.x, p2.y);
                break;
            }
        case pt_cubicto:
            {
                const auto* p = static_cast<const cubic_to_node*>(n);
                const vec2& p1 = p->get_control1();
                const vec2& p2 = p->get_control2();
                const vec2& p3 = p->get_point();
                trace(_t("@cubicTo %f, %f, %f, %f, %f, %f;\n"), p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
                break;
            }
        }
    });
    trace(_t("@@\n"));
#endif
}

void painter_path::tracing_segments() const
{
#if defined (DEBUG) || defined (_DEBUG)
    trace(_t("@!\n"));
    const node* last = 0;
    for(const auto* n : _nodelist) {
        assert(n);
        switch(n->get_tag())
        {
        case pt_moveto:
            {
                last = n;
                break;
            }
        case pt_lineto:
            {
                assert(last);
                const auto* p = static_cast<const line_to_node*>(n);
                const vec2& p0 = last->get_point();
                const vec2& p1 = p->get_point();
                trace(_t("@moveTo %f, %f;\n"), p0.x, p0.y);
                trace(_t("@lineTo %f, %f;\n"), p1.x, p1.y);
                last = n;
                break;
            }
        case pt_quadto:
            {
                assert(last);
                const auto* p = static_cast<const quad_to_node*>(n);
                const vec2& p0 = last->get_point();
                const vec2& p1 = p->get_control();
                const vec2& p2 = p->get_point();
                trace(_t("@moveTo %f, %f;\n"), p0.x, p0.y);
                trace(_t("@quadraticTo %f, %f, %f, %f;\n"), p1.x, p1.y, p2.x, p2.y);
                last = n;
                break;
            }
        case pt_cubicto:
            {
                assert(last);
                const auto* p = static_cast<const cubic_to_node*>(n);
                const vec2& p0 = last->get_point();
                const vec2& p1 = p->get_control1();
                const vec2& p2 = p->get_control2();
                const vec2& p3 = p->get_point();
                trace(_t("@moveTo %f, %f;\n"), p0.x, p0.y);
                trace(_t("@cubicTo %f, %f, %f, %f, %f, %f;\n"), p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
                last = n;
                break;
            }
        }
    }
    trace(_t("@@\n"));
#endif
}

int path_info::get_order() const
{
    assert(node[0] && node[1]);
    auto type = node[1]->get_tag();
    switch(type)
    {
    case painter_path::pt_lineto:
        return 1;
    case painter_path::pt_quadto:
        return 2;
    case painter_path::pt_cubicto:
        return 3;
    default:
        assert(!"unexpected.");
        return 0;
    }
}

int path_info::get_point_count() const
{
    assert(node[0] && node[1]);
    int order = get_order();
    return order ? order + 1 : 0;
}

bool path_info::get_points(vec2 pt[], int cnt) const
{
    assert(node[0] && node[1]);
    assert(pt);
    auto type = node[1]->get_tag();
    if(type == painter_path::pt_lineto) {
        if(cnt < 2)
            return false;
        pt[0] = node[0]->get_point();
        pt[1] = node[1]->get_point();
        return true;
    }
    else if(type == painter_path::pt_quadto) {
        if(cnt < 3)
            return false;
        static_cast_as(const painter_path::quad_to_node*, qn, node[1]);
        pt[0] = node[0]->get_point();
        pt[1] = qn->get_control();
        pt[2] = qn->get_point();
        return true;
    }
    else if(type == painter_path::pt_cubicto) {
        if(cnt < 4)
            return false;
        static_cast_as(const painter_path::cubic_to_node*, cn, node[1]);
        pt[0] = node[0]->get_point();
        pt[1] = cn->get_control1();
        pt[2] = cn->get_control2();
        pt[3] = cn->get_point();
        return true;
    }
    return false;
}

void path_info::tracing() const
{
    vec2 p[4];
    int c = get_point_count();
    get_points(p, _countof(p));
    assert(c >= 2 && c <= 4);
    trace(_t("@!\n"));
    trace(_t("@moveTo %f, %f;\n"), p[0].x, p[0].y);
    switch(c)
    {
    case 2:
        trace(_t("@lineTo %f, %f;\n"), p[1].x, p[1].y);
        break;
    case 3:
        trace(_t("@quadTo %f, %f, %f, %f;\n"), p[1].x, p[1].y, p[2].x, p[2].y);
        break;
    case 4:
        trace(_t("@cubicTo %f, %f, %f, %f, %f, %f;\n"), p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
        break;
    }
    trace(_t("@@\n"));
}

curve_spliter::curve_spliter()
{
    ratio = 0.f;
    child[0] = child[1] = 0;
    parent = 0;
}

curve_spliter::~curve_spliter()
{
    if(child[0]) {
        delete child[0];
        child[0] = 0;
    }
    if(child[1]) {
        delete child[1];
        child[1] = 0;
    }
}

bool curve_spliter::is_leaf() const
{
    if(child[0]) {
        assert(child[1]);
        return false;
    }
    assert(!child[1]);
    return true;
}

curve_spliter_quad::curve_spliter_quad(const vec2 p[3])
{
    assert(p);
    memmove_s(cp, sizeof(cp), p, sizeof(cp));
    get_quad_parameter_equation(para, p[0], p[1], p[2]);
}

bool curve_spliter_quad::get_points(vec2 p[], int count) const
{
    assert(p);
    if(count < 3)
        return false;
    memmove_s(p, sizeof(cp), cp, sizeof(cp));
    return true;
}

void curve_spliter_quad::split(float t)
{
    assert(is_leaf());
    assert(t > 0.f && t < 1.f);
    ratio = t;
    vec2 c[5];
    split_quad_bezier(c, cp, t);
    fixedpt = c[2];
    child[0] = new curve_spliter_quad(c);
    child[1] = new curve_spliter_quad(c + 2);
    child[0]->parent = this;
    child[1]->parent = this;
}

void curve_spliter_quad::interpolate(vec2& p, float t) const
{
    eval_quad(p, para, t);
}

float curve_spliter_quad::reparameterize(const vec2& p) const
{
    return quad_reparameterize(para, p);
}

void curve_spliter_quad::tracing() const
{
    trace(_t("@!\n"));
    trace(_t("@moveTo %f, %f;\n"), cp[0].x, cp[0].y);
    trace(_t("@quadTo %f, %f, %f, %f;\n"), cp[1].x, cp[1].y, cp[2].x, cp[2].y);
    trace(_t("@@\n"));
}

curve_spliter_cubic::curve_spliter_cubic(const vec2 p[4])
{
    assert(p);
    memmove_s(cp, sizeof(cp), p, sizeof(cp));
    get_cubic_parameter_equation(para, p[0], p[1], p[2], p[3]);
}

bool curve_spliter_cubic::get_points(vec2 p[], int count) const
{
    assert(p);
    if(count < 4)
        return false;
    memmove_s(p, sizeof(cp), cp, sizeof(cp));
    return true;
}

void curve_spliter_cubic::split(float t)
{
    assert(is_leaf());
    assert(t > 0.f && t < 1.f);
    ratio = t;
    vec2 c[7];
    split_cubic_bezier(c, cp, t);
    fixedpt = c[3];
    child[0] = new curve_spliter_cubic(c);
    child[1] = new curve_spliter_cubic(c + 3);
    child[0]->parent = this;
    child[1]->parent = this;
}

void curve_spliter_cubic::interpolate(vec2& p, float t) const
{
    eval_cubic(p, para, t);
}

float curve_spliter_cubic::reparameterize(const vec2& p) const
{
    return cubic_reparameterize(para, p);
}

void curve_spliter_cubic::tracing() const
{
    trace(_t("@!\n"));
    trace(_t("@moveTo %f, %f;\n"), cp[0].x, cp[0].y);
    trace(_t("@cubicTo %f, %f, %f, %f, %f, %f;\n"), cp[1].x, cp[1].y, cp[2].x, cp[2].y, cp[3].x, cp[3].y);
    trace(_t("@@\n"));
}

curve_spliter* curve_helper::create_spliter(const path_info* pnf)
{
    assert(pnf);
    int order = pnf->get_order();
    if(order == 2) {
        vec2 p[3];
        pnf->get_points(p, 3);
        return new curve_spliter_quad(p);
    }
    else if(order == 3) {
        vec2 p[4];
        pnf->get_points(p, 4);
        return new curve_spliter_cubic(p);
    }
    return 0;
}

curve_spliter* curve_helper::create_next_spliter(vec2& p, curve_spliter* cs, float t)
{
    assert(cs);
    assert(t > 0.f && t < 1.f);
    vec2 v;
    cs->interpolate(v, t);
    p = v;
    curve_spliter* qcs = query_spliter(cs, t);
    assert(qcs);
    if(!qcs->is_leaf()) {
        vec2 cp[4];
        int count = qcs->get_point_count();
        qcs->get_points(cp, 4);
        const vec2& front = cp[0];
        const vec2& back = cp[count - 1];
        if(p == front || p == back)
            return qcs;
        assert(!"unexpected.");
        return 0;
    }
    float s = qcs->reparameterize(v);
    qcs->split(s);
    return qcs;
}

curve_spliter* curve_helper::query_spliter(curve_spliter* cs, float t)
{
    assert(cs);
    vec2 p;
    cs->interpolate(p, t);
    return query_spliter(cs, p);
}

curve_spliter* curve_helper::query_spliter(curve_spliter* cs, const vec2& p)
{
    assert(cs);
    if(cs->is_leaf())
        return cs;
    assert(cs->child[0] && cs->child[1]);
    float t = cs->reparameterize(p);
    return t < cs->ratio ? query_spliter(cs->child[0], p) :
        query_spliter(cs->child[1], p);
}

curve_spliter* curve_helper::query_spliter(curve_spliter* cs, const vec2& p1, const vec2& p2)
{
    assert(cs);
    if(cs->is_leaf())
        return cs;
    assert(cs->child[0] && cs->child[1]);
    float t1 = cs->reparameterize(p1);
    if(abs(cs->ratio - t1) > 0.001f) {
        return t1 < cs->ratio ? query_spliter(cs->child[0], p1, p2) :
            query_spliter(cs->child[1], p1, p2);
    }
    float t2 = cs->reparameterize(p2);
    assert(abs(cs->ratio - t2) > 0.001f);
    return t2 < cs->ratio ? query_spliter(cs->child[0], p1, p2) :
        query_spliter(cs->child[1], p1, p2);
}

static void check_last_path_segment(painter_path& output, uint mask)
{
    int size = output.size();
    if(size <= 0)
        return;
    auto* lastnode = output.get_node(size - 1);
    assert(lastnode);
    if(mask & painter_helper::reduce_short_line) {
        if(lastnode->get_tag() == painter_path::pt_lineto) {
            if(size >= 2) {
                auto* lastnode2 = output.get_node(size - 2);
                assert(lastnode2);
                auto& p1 = lastnode2->get_point();
                auto& p2 = lastnode->get_point();
                float len = vec2().sub(p2, p1).length();
                if(len < 1.f) {     /* remove the line less than 1 pixels */
                    output.resize(size - 1);
                    return;     /* no need to do merge straight */
                }
            }
        }
    }
    if(mask & painter_helper::merge_straight_line) {
        if(lastnode->get_tag() == painter_path::pt_lineto) {
            if(size >= 3) {
                auto* lastnode2 = output.get_node(size - 2);
                auto* lastnode3 = output.get_node(size - 3);
                assert(lastnode2 && lastnode3);
                if(lastnode2->get_tag() == painter_path::pt_lineto) {
                    if(is_approx_line(lastnode3->get_point(), lastnode2->get_point(), lastnode->get_point(), 0.055f)) {
                        output.resize(-- size);
                        lastnode = output.get_node(size - 1);
                    }
                }
            }
        }
    }
}

static void transform_path_line(painter_path& output, const painter_path& path, int i, const painter_path::line_to_node* node, uint mask)
{
    if(mask & painter_helper::merge_straight_line) {
        if(i > 1) {
            const auto* node1 = path.get_node(i - 1);
            assert(node1);
            if(node1->get_tag() == painter_path::pt_lineto) {
                const auto* node2 = path.get_node(i - 2);
                assert(node2);
                if(is_approx_line(node2->get_point(), node1->get_point(), node->get_point(), 0.055f)) {
                    auto* modify = output.get_node(output.size() - 1);
                    assert(modify && modify->get_tag() == painter_path::pt_lineto);
                    modify->set_point(node->get_point());
                    return;
                }
            }
        }
    }
    output.line_to(node->get_point());
}

static void transform_path_quad(painter_path& output, const painter_path& path, int i, const painter_path::quad_to_node* node, uint mask)
{
    if(mask & painter_helper::reduce_straight_curve) {
        assert(i > 0);
        auto* lastnode = path.get_node(i - 1);
        assert(lastnode);
        if(is_approx_line(lastnode->get_point(), node->get_control(), node->get_point(), 0.055f)) {
            output.line_to(node->get_point());
            return;
        }
    }
    output.quad_to(node->get_control(), node->get_point());
}

static void transform_path_cubic(painter_path& output, const painter_path& path, int i, const painter_path::cubic_to_node* node, uint mask)
{
    if(mask & painter_helper::reduce_straight_curve) {
        assert(i > 0);
        auto* lastnode = path.get_node(i - 1);
        assert(lastnode);
        float d1 = point_line_distance(node->get_control1(), lastnode->get_point(), node->get_point());
        float d2 = point_line_distance(node->get_control2(), lastnode->get_point(), node->get_point());
        if(abs(d1) < 0.2f && abs(d2) < 0.2f) {
            output.line_to(node->get_point());
            return;
        }
    }
    if(!(mask & (painter_helper::fix_loop | painter_helper::fix_inflection))) {
        output.cubic_to(node->get_control1(), node->get_control2(), node->get_point());
        return;
    }
    vec2 p[4];
    const auto* node1 = path.get_node(i - 1);
    assert(node1);
    p[0] = node1->get_point();
    p[1] = node->get_control1();
    p[2] = node->get_control2();
    p[3] = node->get_point();
    if(mask & painter_helper::fix_loop) {
        float st[2];
        if(get_self_intersection(st, p[0], p[1], p[2], p[3])) {
            vec2 sp1[10];
            split_cubic_bezier(sp1, p, st[0], st[1]);
            if(mask & painter_helper::fix_inflection) {
                float t[2];
                for(int j = 0; j <= 6; j += 3) {
                    int c = get_cubic_inflection(t, sp1[j], sp1[j + 1], sp1[j + 2], sp1[j + 3]);
                    if(c == 1) {
                        vec2 sp2[7];
                        split_cubic_bezier(sp2, sp1 + j, t[0]);
                        output.cubic_to(sp2[1], sp2[2], sp2[3]);
                        output.cubic_to(sp2[4], sp2[5], sp2[6]);
                    }
                    else {
                        assert(!c);
                        output.cubic_to(sp1[j + 1], sp1[j + 2], sp1[j + 3]);
                    }
                }
            }
            else {
                output.cubic_to(sp1[1], sp1[2], sp1[3]);
                output.cubic_to(sp1[4], sp1[5], sp1[6]);
                output.cubic_to(sp1[7], sp1[8], sp1[9]);
            }
            return;
        }
    }
    if(mask & painter_helper::fix_inflection) {
        float st[2];
        int c = get_cubic_inflection(st, p[0], p[1], p[2], p[3]);
        if(c == 1) {
            vec2 sp[7];
            split_cubic_bezier(sp, p, st[0]);
            output.cubic_to(sp[1], sp[2], sp[3]);
            output.cubic_to(sp[4], sp[5], sp[6]);
        }
        else if(c == 2) {
            vec2 sp[10];
            split_cubic_bezier(sp, p, st[0], st[1]);
            output.cubic_to(sp[1], sp[2], sp[3]);
            output.cubic_to(sp[4], sp[5], sp[6]);
            output.cubic_to(sp[7], sp[8], sp[9]);
        }
        else {
            assert(!c);
            output.cubic_to(p[1], p[2], p[3]);
        }
        return;
    }
    output.cubic_to(p[1], p[2], p[3]);
}

void painter_helper::transform(painter_path& output, const painter_path& path, uint mask)
{
    int c = path.size();
    for(int i = 0; i < c; i ++) {
        const painter_node* node = path.get_node(i);
        assert(node);
        switch(node->get_tag())
        {
        case painter_path::pt_moveto:
            output.move_to(node->get_point());
            break;
        case painter_path::pt_lineto:
            transform_path_line(output, path, i, node->as_const_node<painter_path::line_to_node>(), mask);
            break;
        case painter_path::pt_quadto:
            transform_path_quad(output, path, i, node->as_const_node<painter_path::quad_to_node>(), mask);
            break;
        case painter_path::pt_cubicto:
            transform_path_cubic(output, path, i, node->as_const_node<painter_path::cubic_to_node>(), mask);
            break;
        }
        check_last_path_segment(output, mask);
    }
}

static int close_sub_path(painter_path& output, const painter_path& path, int start)
{
    int cap = path.size();
    if(start >= cap)
        return cap;
    const painter_node* first = path.get_node(start);
    assert(first && first->get_tag() == painter_path::pt_moveto);
    output.move_to(first->get_point());
    int i = start + 1;
    for(; i < cap; i ++) {
        const painter_node* node = path.get_node(i);
        assert(node);
        auto tag = node->get_tag();
        if(tag == painter_path::pt_moveto)
            break;
        switch(tag)
        {
        case painter_path::pt_lineto:
            output.line_to(node->get_point());
            break;
        case painter_path::pt_quadto:
            output.quad_to(
                static_cast<const painter_path::quad_to_node*>(node)->get_control(),
                node->get_point()
                );
            break;
        case painter_path::pt_cubicto:
            output.cubic_to(
                static_cast<const painter_path::cubic_to_node*>(node)->get_control1(),
                static_cast<const painter_path::cubic_to_node*>(node)->get_control2(),
                node->get_point()
                );
            break;
        default:
            assert(!"unexpected.");
            break;
        }
    }
    if(i - start == 1) {
        assert(!"empty move to.");
        return i;
    }
    const painter_node* last = path.get_node(i - 1);
    assert(first != last);
    if(first->get_point() != last->get_point())
        output.line_to(first->get_point());
    return i;
}

void painter_helper::close_sub_paths(painter_path& output, const painter_path& path)
{
    int cap = path.size();
    if(!cap)
        return;
    for(int i = close_sub_path(output, path, 0); i < cap; i = close_sub_path(output, path, i));
}

void raster::save()
{
    _ctxst.push(_context);
}

void raster::restore()
{
    if(_ctxst.empty())
        return;
    _context = _ctxst.top();
    _ctxst.pop();
}

__pink_end__
