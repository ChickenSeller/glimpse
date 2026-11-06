/*
 * Copyright (C) 2026 Glimpse contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "CurveResizeHandles.h"

namespace kImageAnnotator {

CurveResizeHandles::CurveResizeHandles(AbstractAnnotationCurve *curveItem, double zoomValue) :
	mCurveItem(curveItem)
{
	initHandles(2, zoomValue);
	update();
}

void CurveResizeHandles::update()
{
	// One handle per bend point besides the ends; bend points come and go.
	// New ones copy an end's handle, which has the size for the zoom.
	const int count = mCurveItem->pointCount();
	while (mHandles.size() < count) {
		mHandles.append(mHandles.first());
	}
	while (mHandles.size() > count) {
		mHandles.removeLast();
	}
	initCursors();

	// The ends just beyond the line's own ends, as for a line.
	auto line = mCurveItem->line();
	auto offset = mCurveItem->properties()->width();
	auto outerLine = ShapeHelper::extendLine(line, offset);
	mHandles[0].moveCenter(outerLine.p1());
	mHandles[0].setAnchor(line.p1());
	mHandles[1].moveCenter(outerLine.p2());
	mHandles[1].setAnchor(line.p2());
	// The bend points right on the curve, and the active one's handles.
	for (int i = 2; i < count; ++i) {
		const QPointF point = mCurveItem->pointAt(i);
		mHandles[i].moveCenter(point);
		mHandles[i].setAnchor(point);
	}
}

void CurveResizeHandles::initCursors()
{
	mCursors.clear();
	for (int i = 0; i < mHandles.size(); ++i) {
		mCursors.append(CursorHelper::allResizeCursor());
	}
}

} // namespace kImageAnnotator
