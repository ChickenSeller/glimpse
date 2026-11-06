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
#include <limits>

namespace kImageAnnotator {

namespace {

// The arrow's shaft stops this far short of its tip, so that the round cap
// of a thick line does not show through the head (as for straight arrows).
constexpr qreal kShaftShortening = 5;
// Steps per segment when finding the place on the curve nearest a point.
constexpr int kSamples = 32;

qreal dot(const QPointF &a, const QPointF &b)
{
	return a.x() * b.x() + a.y() * b.y();
}

QPointF cubicAt(const QPointF &p0, const QPointF &c1, const QPointF &c2, const QPointF &p1, qreal t)
{
	const qreal u = 1 - t;
	return u * u * u * p0 + 3 * u * u * t * c1 + 3 * u * t * t * c2 + t * t * t * p1;
}

qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
	const QPointF ab = b - a;
	const qreal length2 = dot(ab, ab);
	const qreal t = qFuzzyIsNull(length2) ? 0 : std::clamp(dot(p - a, ab) / length2, 0.0, 1.0);
	return QLineF(p, a + t * ab).length();
}

} // namespace

AbstractAnnotationCurve::AbstractAnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties) :
	AbstractAnnotationLine(startPosition, properties)
{
}

AbstractAnnotationCurve::AbstractAnnotationCurve(const AbstractAnnotationCurve &other) :
	AbstractAnnotationLine(other),
	mBends(other.mBends)
{
}

QPointF AbstractAnnotationCurve::toPoint(const QPointF &bend) const
{
	const QPointF start = mLine->p1();
	const QPointF chord = mLine->p2() - start;
	const QPointF left(-chord.y(), chord.x()); // as long as the chord
	return start + bend.x() * chord + bend.y() * left;
}

std::optional<QPointF> AbstractAnnotationCurve::toBend(const QPointF &point) const
{
	const QPointF chord = mLine->p2() - mLine->p1();
	const qreal length2 = dot(chord, chord);
	if (qFuzzyIsNull(length2)) {
		return std::nullopt; // no line to bend yet
	}
	const QPointF offset = point - mLine->p1();
	return QPointF(dot(offset, chord) / length2, dot(offset, QPointF(-chord.y(), chord.x())) / length2);
}

QList<QPointF> AbstractAnnotationCurve::curvePoints() const
{
	QList<QPointF> points{mLine->p1()};
	for (const QPointF &bend : mBends) {
		points.append(toPoint(bend));
	}
	points.append(mLine->p2());
	return points;
}

QList<QPointF> AbstractAnnotationCurve::controlPoints(const QList<QPointF> &points) const
{
	// Catmull-Rom: each segment leaves a point in the direction from the
	// point before it to the point after it, so the curve passes through
	// every point without a kink. The ends repeat themselves as neighbours.
	QList<QPointF> controls;
	const int last = int(points.size()) - 1;
	for (int i = 0; i < last; ++i) {
		const QPointF &before = points[std::max(i - 1, 0)];
		const QPointF &from = points[i];
		const QPointF &to = points[i + 1];
		const QPointF &after = points[std::min(i + 2, last)];
		controls.append(from + (to - before) / 6);
		controls.append(to - (after - from) / 6);
	}
	return controls;
}

int AbstractAnnotationCurve::bendCount() const
{
	return int(mBends.size());
}

QList<QPointF> AbstractAnnotationCurve::bends() const
{
	return mBends;
}

void AbstractAnnotationCurve::setBends(const QList<QPointF> &bends)
{
	prepareGeometryChange();
	mBends = bends;
	updateShape();
}

std::optional<QList<QPointF>> AbstractAnnotationCurve::bendsWithPointAt(const QPointF &pos, qreal tolerance) const
{
	const std::optional<QPointF> bend = toBend(pos);
	if (!bend) {
		return std::nullopt;
	}
	// The segment the point is nearest to, from points sampled along each.
	const QList<QPointF> points = curvePoints();
	const QList<QPointF> controls = controlPoints(points);
	int nearestSegment = -1;
	qreal nearest = std::numeric_limits<qreal>::max();
	for (int i = 0; i + 1 < points.size(); ++i) {
		QPointF previous = points[i];
		for (int step = 1; step <= kSamples; ++step) {
			const QPointF next = cubicAt(points[i], controls[2 * i], controls[2 * i + 1], points[i + 1], qreal(step) / kSamples);
			const qreal distance = distanceToSegment(pos, previous, next);
			if (distance < nearest) {
				nearest = distance;
				nearestSegment = i;
			}
			previous = next;
		}
	}
	if (nearestSegment < 0 || nearest > tolerance) {
		return std::nullopt;
	}
	// Segment i runs from point i to point i + 1; bend point k is point k + 1.
	QList<QPointF> bends = mBends;
	bends.insert(nearestSegment, *bend);
	return bends;
}

std::optional<QList<QPointF>> AbstractAnnotationCurve::bendsWithoutPoint(int index) const
{
	const int bend = index - 2;
	if (bend < 0 || bend >= mBends.size()) {
		return std::nullopt;
	}
	QList<QPointF> bends = mBends;
	bends.removeAt(bend);
	return bends;
}

void AbstractAnnotationCurve::setPointAt(const QPointF &point, int index, bool snapToAngle)
{
	if (index < 2) {
		// The ends: the bend points keep their places relative to them.
		AbstractAnnotationLine::setPointAt(point, index, snapToAngle);
		return;
	}
	const int bend = index - 2;
	const std::optional<QPointF> relative = toBend(point);
	if (bend >= mBends.size() || !relative) {
		return;
	}
	prepareGeometryChange();
	mBends[bend] = *relative;
	updateShape();
}

QPointF AbstractAnnotationCurve::pointAt(int index) const
{
	if (index < 2) {
		return AbstractAnnotationLine::pointAt(index);
	}
	const int bend = std::min(index - 2, int(mBends.size()) - 1);
	return bend < 0 ? mLine->center() : toPoint(mBends[bend]);
}

// Selected right after it is drawn, so that it can be bent at once.
bool AbstractAnnotationCurve::requiresSelectionAfterCreation() const
{
	return true;
}

// Arrow heads and their size come from the properties.
void AbstractAnnotationCurve::setProperties(const PropertiesPtr &properties)
{
	AbstractAnnotationLine::setProperties(properties);
	updateShape();
}

void AbstractAnnotationCurve::updateShape()
{
	QList<QPointF> points = curvePoints();
	const QList<QPointF> controls = controlPoints(points);
	const auto heads = properties()->arrowHeads();

	// A head at `tip`, pointing along the curve away from `inner`; the shaft
	// then stops a little short of the tip.
	mHeads = QPainterPath();
	const auto addHead = [this, &points](int tipIndex, const QPointF &inner, const QPointF &fallback) {
		const QPointF tip = points[tipIndex];
		QLineF tangent(inner, tip);
		if (tangent.length() < 1) {
			tangent = QLineF(fallback, tip);
		}
		auto head = AnnotationShapeCreator::createArrowHead(properties()->width() / 2);
		head = AnnotationShapeCreator::translate(head, tip, -tangent.angle());
		mHeads.addPolygon(head);
		mHeads.closeSubpath();

		QLineF shortened(tip, tangent.p1());
		shortened.setLength(std::min(kShaftShortening, tangent.length()));
		points[tipIndex] = shortened.p2();
	};
	const int last = int(points.size()) - 1;
	if (heads == ArrowHeads::End || heads == ArrowHeads::Both) {
		addHead(last, controls.last(), points[last - 1]);
	}
	if (heads == ArrowHeads::Start || heads == ArrowHeads::Both) {
		addHead(0, controls.first(), points[1]);
	}

	mCurve = QPainterPath(points.first());
	for (int i = 0; i < last; ++i) {
		mCurve.cubicTo(controls[2 * i], controls[2 * i + 1], points[i + 1]);
	}

	QPainterPath shape = mCurve;
	shape.addPath(mHeads);
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
	if (!mHeads.isEmpty()) {
		painter->setBrush(painterPen().color());
		painter->drawPath(mHeads);
	}
}

} // namespace kImageAnnotator
