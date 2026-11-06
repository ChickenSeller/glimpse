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

namespace kImageAnnotator {

// A line that bends: drawn straight like a line, then bent by its third
// point (index 2), the bend point, which the curve passes through halfway.
// The curve is a quadratic Bezier curve; the bend point is kept relative to
// the two ends, so it follows them when they are moved, and the whole item
// when it is moved or scaled.
class AbstractAnnotationCurve : public AbstractAnnotationLine
{
public:
	AbstractAnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties);
	AbstractAnnotationCurve(const AbstractAnnotationCurve &other);
	~AbstractAnnotationCurve() override = default;
	void setPointAt(const QPointF &point, int index, bool snapToAngle) override;
	QPointF pointAt(int index) const override;
	QPointF bendPoint() const;

protected:
	void updateShape() override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
	virtual bool hasArrowHead() const = 0;

private:
	// Where the bend point is: a share of the way from the start to the end,
	// and a share of that length to the left of it (0 = on the line).
	qreal mBendAlong;
	qreal mBendAcross;
	QPainterPath mCurve; // stroked only
	QPainterPath mHead;  // stroked and filled

	QPointF controlPoint() const;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_ABSTRACTANNOTATIONCURVE_H
