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

#ifndef KIMAGEANNOTATOR_ABSTRACTANNOTATIONCURVE_H
#define KIMAGEANNOTATOR_ABSTRACTANNOTATIONCURVE_H

#include "AbstractAnnotationLine.h"
#include "src/annotations/items/helper/AnnotationShapeCreator.h"

#include <optional>

namespace kImageAnnotator {

// A line that bends: drawn straight, then bent by its bend points, which the
// curve passes through in order, smoothly (a Catmull-Rom spline, drawn as
// cubic Bezier curves). Bend points are added on the curve and removed
// again. Arrow heads go where its properties say: at the end, the start,
// both or neither.
//
// Points by index: 0 the start, 1 the end, 2 and up the bend points from the
// start on. The bend points are kept relative to the two ends, so they
// follow them when they are moved, and the whole item when it is moved or
// scaled.
class AbstractAnnotationCurve : public AbstractAnnotationLine
{
public:
	AbstractAnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties);
	AbstractAnnotationCurve(const AbstractAnnotationCurve &other);
	~AbstractAnnotationCurve() override = default;
	void setPointAt(const QPointF &point, int index, bool snapToAngle) override;
	QPointF pointAt(int index) const override;
	bool requiresSelectionAfterCreation() const override;
	void setProperties(const PropertiesPtr &properties) override;

	int bendCount() const;
	// The bend points in the form kept (relative to the ends), to restore them.
	QList<QPointF> bends() const;
	void setBends(const QList<QPointF> &bends);
	// The bends with one more at `pos`, if it is within `tolerance` of the
	// curve, between the points it lies between.
	std::optional<QList<QPointF>> bendsWithPointAt(const QPointF &pos, qreal tolerance) const;
	// The bends without the one of point `index` (2 and up).
	std::optional<QList<QPointF>> bendsWithoutPoint(int index) const;

protected:
	void updateShape() override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
	// Each bend point as a share of the way from the start to the end, and a
	// share of that length to the left of it (0 = on the straight line).
	QList<QPointF> mBends;
	QPainterPath mCurve; // stroked only
	QPainterPath mHeads; // stroked and filled

	QPointF toPoint(const QPointF &bend) const;
	std::optional<QPointF> toBend(const QPointF &point) const;
	// Start, bend points, end.
	QList<QPointF> curvePoints() const;
	// The cubic Bezier control points of each segment (two per segment).
	QList<QPointF> controlPoints(const QList<QPointF> &points) const;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_ABSTRACTANNOTATIONCURVE_H
