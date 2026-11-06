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

#include "AbstractAnnotationCurve.h"

#include <algorithm>

namespace kImageAnnotator {

namespace {

// The arrow's shaft stops this far short of its tip, so that the round cap
// of a thick line does not show through the head (as for straight arrows).
constexpr qreal kShaftShortening = 5;

qreal dot(const QPointF &a, const QPointF &b)
{
	return a.x() * b.x() + a.y() * b.y();
}

} // namespace

AbstractAnnotationCurve::AbstractAnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties) :
	AbstractAnnotationLine(startPosition, properties),
	mBendAlong(0.5),
	mBendAcross(0)
{
}

AbstractAnnotationCurve::AbstractAnnotationCurve(const AbstractAnnotationCurve &other) :
	AbstractAnnotationLine(other),
	mBendAlong(other.mBendAlong),
	mBendAcross(other.mBendAcross)
{
}

QPointF AbstractAnnotationCurve::bendPoint() const
{
	const QPointF start = mLine->p1();
	const QPointF chord = mLine->p2() - start;
	const QPointF left(-chord.y(), chord.x()); // as long as the chord
	return start + mBendAlong * chord + mBendAcross * left;
}

// The Bezier control point that makes the curve pass through the bend point
// halfway (t = 0.5).
QPointF AbstractAnnotationCurve::controlPoint() const
{
	return 2 * bendPoint() - (mLine->p1() + mLine->p2()) / 2;
}

void AbstractAnnotationCurve::setPointAt(const QPointF &point, int index, bool snapToAngle)
{
	if (index < 2) {
		// The ends: the bend point keeps its place relative to them.
		AbstractAnnotationLine::setPointAt(point, index, snapToAngle);
		return;
	}
	const QPointF chord = mLine->p2() - mLine->p1();
	const qreal length2 = dot(chord, chord);
	if (qFuzzyIsNull(length2)) {
		return; // no line to bend yet
	}
	prepareGeometryChange();
	const QPointF offset = point - mLine->p1();
	mBendAlong = dot(offset, chord) / length2;
	mBendAcross = dot(offset, QPointF(-chord.y(), chord.x())) / length2;
	updateShape();
}

QPointF AbstractAnnotationCurve::pointAt(int index) const
{
	return index < 2 ? AbstractAnnotationLine::pointAt(index) : bendPoint();
}

void AbstractAnnotationCurve::updateShape()
{
	const QPointF start = mLine->p1();
	const QPointF end = mLine->p2();
	const QPointF control = controlPoint();

	mCurve = QPainterPath(start);
	mHead = QPainterPath();
	if (!hasArrowHead()) {
		mCurve.quadTo(control, end);
	} else {
		// The head points along the curve at its end.
		QLineF tangent(control, end);
		if (tangent.length() < 1) {
			tangent = QLineF(start, end);
		}
		auto head = AnnotationShapeCreator::createArrowHead(properties()->width() / 2);
		head = AnnotationShapeCreator::translate(head, end, -tangent.angle());
		mHead.addPolygon(head);
		mHead.closeSubpath();

		QLineF tip(end, tangent.p1());
		tip.setLength(std::min(kShaftShortening, tangent.length()));
		mCurve.quadTo(control, tip.p2());
	}

	QPainterPath shape = mCurve;
	shape.addPath(mHead);
	setShape(shape);
}

void AbstractAnnotationCurve::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	// Not AbstractAnnotationItem's painting: with a fill, it would fill the
	// area between the curve and the straight line between its ends.
	adjustPainter(painter);
	painter->setPen(painterPen());
	painter->setBrush(Qt::NoBrush);
	painter->drawPath(mCurve);
	if (!mHead.isEmpty()) {
		painter->setBrush(painterPen().color());
		painter->drawPath(mHead);
	}
}

} // namespace kImageAnnotator
