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

#ifndef KIMAGEANNOTATOR_CURVERESIZEHANDLES_H
#define KIMAGEANNOTATOR_CURVERESIZEHANDLES_H

#include "AbstractItemResizeHandles.h"
#include "src/annotations/items/AbstractAnnotationCurve.h"
#include "src/common/helper/ShapeHelper.h"

namespace kImageAnnotator {

// A curve's two ends, as for a line, and its bend points between them,
// which bend the curve when dragged.
class CurveResizeHandles : public AbstractItemResizeHandles
{
public:
    CurveResizeHandles(AbstractAnnotationCurve *curveItem, double zoomValue);
    ~CurveResizeHandles() override = default;
    void update() override;

protected:
    void initCursors() override;

private:
    AbstractAnnotationCurve *mCurveItem;
};

} // namespace kImageAnnotator

#endif //KIMAGEANNOTATOR_CURVERESIZEHANDLES_H
