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

#ifndef rectpack_89b58399_0f97_4cb9_9160_0226504762b1_h
#define rectpack_89b58399_0f97_4cb9_9160_0226504762b1_h

#include <ariel/config.h>
#include <gslib/bintree.h>
#include <gslib/std.h>
#include <gslib/math.h>
#include <pink/type.h>

__ariel_begin__

struct rp_rect
{
    float               x, y;                           /* left, top */
    float               width, height;

public:
    rp_rect() { x = y = width = height = 0.f; }
    rp_rect(float l, float t, float w, float h) { set_rect(l, t, w, h); }
    void set_rect(float l, float t, float w, float h) { x = l, y = t, width = w, height = h; }
    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + width; }
    float bottom() const { return y + height; }
    void move_to(float destx, float desty) { x = destx, y = desty; }
    void offset(float cx, float cy) { x += cx, y += cy; }
    float area() const { return width * height; }
};

struct rp_node:
    public rp_rect
{
    bool                transposed;
    void*               bind_ptr;

public:
    rp_node()
    {
        transposed = false;
        bind_ptr = nullptr;
    }
    void set_transposed(bool b) { transposed = b; }
    void set_bind_ptr(void* p) { bind_ptr = p; }
    bool is_empty() const { return !bind_ptr; }
};

typedef _bintreenode_wrapper<rp_node> rp_wrapper;
typedef _bintree_allocator<rp_wrapper> rp_allocator;
typedef bintree<rp_node, rp_wrapper, rp_allocator> rp_tree;
typedef rp_tree::iterator rp_iterator;
typedef rp_tree::const_iterator rp_const_iterator;

struct rp_input
{
    float               width, height;
    void*               binding;
};

class rect_packer
{
public:
    rect_packer();
    ~rect_packer();
    void initialize(float w, float h);
    void tracing() const;
    const rp_node& get_root_node() const { return *_tree.const_root(); }

public:
    template<class _inputlist>
    bool pack(const _inputlist& inputs)
    {
        for(const auto& rc : inputs) {
            if(!add_rect(rc.width, rc.height, rc.binding))
                return false;
        }
        return true;
    }

protected:
    rp_tree             _tree;

protected:
    bool add_rect(float w, float h, void* binding);
};

__ariel_end__

#endif
