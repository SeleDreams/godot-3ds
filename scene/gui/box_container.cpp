/*************************************************************************/
/*  box_container.cpp                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "box_container.h"
#include "label.h"
#include "margin_container.h"

struct _MinSizeCache {

	int min_size;
	bool will_stretch;
	int final_size;
};

void BoxContainer::_resort() {

	/** First pass, determine minimum size AND amount of stretchable elements */

	Size2i new_size = get_size();

	int sep = get_constant("separation"); //,vertical?"VBoxContainer":"HBoxContainer");

	bool first = true;
	int children_count = 0;
	int stretch_min = 0;
	int stretch_avail = 0;
	float stretch_ratio_total = 0;
	Map<Control *, _MinSizeCache> min_size_cache;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = get_child(i)->cast_to<Control>();
		if (!c || !c->is_visible())
			continue;
		if (c->is_set_as_toplevel())
			continue;

		Size2i size = c->get_combined_minimum_size();
		_MinSizeCache msc;

		if (vertical) { /* VERTICAL */
			stretch_min += size.height;
			msc.min_size = size.height;
			msc.will_stretch = c->get_v_size_flags() & SIZE_EXPAND;

		} else { /* HORIZONTAL */
			stretch_min += size.width;
			msc.min_size = size.width;
			msc.will_stretch = c->get_h_size_flags() & SIZE_EXPAND;
		}

		if (msc.will_stretch) {
			stretch_avail += msc.min_size;
			stretch_ratio_total += c->get_stretch_ratio();
		}
		msc.final_size = msc.min_size;
		min_size_cache[c] = msc;
		children_count++;
	}

	if (children_count == 0)
		return;

	int stretch_max = (vertical ? new_size.height : new_size.width) - (children_count - 1) * sep;
	int stretch_diff = stretch_max - stretch_min;
	if (stretch_diff < 0) {
		//avoid negative stretch space
		stretch_max = stretch_min;
		stretch_diff = 0;
	}

	stretch_avail += stretch_diff; //available stretch space.
	/** Second, pass sucessively to discard elements that can't be stretched, this will run while stretchable
		elements exist */

	bool has_stretched = false;
	while (stretch_ratio_total > 0) { // first of all, dont even be here if no stretchable objects exist

		has_stretched = true;
		bool refit_successful = true; //assume refit-test will go well

		for (int i = 0; i < get_child_count(); i++) {

			Control *c = get_child(i)->cast_to<Control>();
			if (!c || !c->is_visible())
				continue;
			if (c->is_set_as_toplevel())
				continue;

			ERR_FAIL_COND(!min_size_cache.has(c));
			_MinSizeCache &msc = min_size_cache[c];

			if (msc.will_stretch) { //wants to stretch
				//let's see if it can really stretch

				int final_pixel_size = stretch_avail * c->get_stretch_ratio() / stretch_ratio_total;
				if (final_pixel_size < msc.min_size) {
					//if available stretching area is too small for widget,
					//then remove it from stretching area
					msc.will_stretch = false;
					stretch_ratio_total -= c->get_stretch_ratio();
					refit_successful = false;
					stretch_avail -= msc.min_size;
					msc.final_size = msc.min_size;
					break;
				} else {
					msc.final_size = final_pixel_size;
				}
			}
		}

		if (refit_successful) //uf refit went well, break
			break;
	}

	/** Final pass, draw and stretch elements **/

	int ofs = 0;
	if (!has_stretched) {
		switch (align) {
			case ALIGN_BEGIN:
				break;
			case ALIGN_CENTER:
				ofs = stretch_diff / 2;
				break;
			case ALIGN_END:
				ofs = stretch_diff;
				break;
		}
	}

	first = true;
	int idx = 0;

	for (int i = 0; i < get_child_count(); i++) {

		Control *c = get_child(i)->cast_to<Control>();
		if (!c || !c->is_visible())
			continue;
		if (c->is_set_as_toplevel())
			continue;

		_MinSizeCache &msc = min_size_cache[c];

		if (first)
			first = false;
		else
			ofs += sep;

		int from = ofs;
		int to = ofs + msc.final_size;

		if (msc.will_stretch && idx == children_count - 1) {
			//adjust so the last one always fits perfect
			//compensating for numerical imprecision

			to = vertical ? new_size.height : new_size.width;
		}

		int size = to - from;

		Rect2 rect;

		if (vertical) {

			rect = Rect2(0, from, new_size.width, size);
		} else {

			rect = Rect2(from, 0, size, new_size.height);
		}

		fit_child_in_rect(c, rect);

		ofs = to;
		idx++;
	}
}

Size2 BoxContainer::get_minimum_size() const {

	/* Calculate MINIMUM SIZE */

	Size2i minimum;
	int sep = get_constant("separation"); //,vertical?"VBoxContainer":"HBoxContainer");

	bool first = true;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = get_child(i)->cast_to<Control>();
		if (!c)
			continue;
		if (c->is_set_as_toplevel())
			continue;

		if (c->is_hidden()) {
			continue;
		}

		Size2i size = c->get_combined_minimum_size();

		if (vertical) { /* VERTICAL */

			if (size.width > minimum.width) {
				minimum.width = size.width;
			}

			minimum.height += size.height + (first ? 0 : sep);

		} else { /* HORIZONTAL */

			if (size.height > minimum.height) {
				minimum.height = size.height;
			}

			minimum.width += size.width + (first ? 0 : sep);
		}

		first = false;
	}

	return minimum;
}

void BoxContainer::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_SORT_CHILDREN: {

			_resort();
		} break;
	}
}

void BoxContainer::set_alignment(AlignMode p_align) {
	align = p_align;
	_resort();
}

BoxContainer::AlignMode BoxContainer::get_alignment() const {
	return align;
}

void BoxContainer::add_spacer(bool p_begin) {

	Control *c = memnew(Control);
	c->set_stop_mouse(false);
	if (vertical)
		c->set_v_size_flags(SIZE_EXPAND_FILL);
	else
		c->set_h_size_flags(SIZE_EXPAND_FILL);

	add_child(c);
	if (p_begin)
		move_child(c, 0);
}

BoxContainer::BoxContainer(bool p_vertical) {

	vertical = p_vertical;
	align = ALIGN_BEGIN;
	//	set_ignore_mouse(true);
	set_stop_mouse(false);
}

void BoxContainer::_bind_methods() {

	ObjectTypeDB::bind_method(_MD("add_spacer", "begin"), &BoxContainer::add_spacer);
	ObjectTypeDB::bind_method(_MD("get_alignment"), &BoxContainer::get_alignment);
	ObjectTypeDB::bind_method(_MD("set_alignment", "alignment"), &BoxContainer::set_alignment);

	BIND_CONSTANT(ALIGN_BEGIN);
	BIND_CONSTANT(ALIGN_CENTER);
	BIND_CONSTANT(ALIGN_END);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Begin,Center,End"), _SCS("set_alignment"), _SCS("get_alignment"));
}

MarginContainer *VBoxContainer::add_margin_child(const String &p_label, Control *p_control, bool p_expand) {

	Label *l = memnew(Label);
	l->set_text(p_label);
	add_child(l);
	MarginContainer *mc = memnew(MarginContainer);
	mc->add_child(p_control);
	add_child(mc);
	if (p_expand)
		mc->set_v_size_flags(SIZE_EXPAND_FILL);

	return mc;
}
