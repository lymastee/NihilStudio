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
#include <ariel/rectpack.h>

__ariel_begin__

rect_packer::rect_packer()
{
}

rect_packer::~rect_packer()
{
}

void rect_packer::initialize(float w, float h)
{
    _tree.destroy();
    auto i = _tree.insertll(rp_iterator(0));
    assert(i);
    i->set_rect(0.f, 0.f, w, h);
}

void rect_packer::tracing() const
{

}

static bool rp_add_rect(rp_tree& t, rp_iterator i, float w, float h, void* binding)
{
    assert(t.is_valid() && i);
    if(i.is_leaf()) {
        if(!i->is_empty())
            return false;
        if(i->width > i->height) {
            if(w > h) {
                if(i->width > ) {
                }
            }
            else {

            }
        }
        else {
        }
    }
    assert(i.left() && i.right());
    return rp_add_rect(t, i.left(), w, h, binding) || rp_add_rect(t, i.right(), w, h, binding);
}

bool rect_packer::add_rect(float w, float h, void* binding)
{
    return rp_add_rect(_tree, _tree.get_root(), w, h, binding);
}



__ariel_end__
