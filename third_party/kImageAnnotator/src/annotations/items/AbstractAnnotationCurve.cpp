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
// Steps per segment when finding the place on the curve nearest a point,
// then refined between the steps next to the nearest one.
constexpr int kSamples = 64;
constexpr int kRefinements = 24;

qreal dot(const QPointF &a, const QPointF &b)
{
	return a.x() * b.x() + a.y() * b.y();
}

QPointF lerp(const QPointF &a, const QPointF &b, qreal t)
{
	return a + t * (b - a);
}

QPointF cubicAt(const QPointF &p0, const QPointF &c1, const QPointF &c2, const QPointF &p1, qreal t)
{
	const qreal u = 1 - t;
	return u * u * u * p0 + 3 * u * u * t * c1 + 3 * u * t * t * c2 + t * t * t * p1;
}

} // namespace

AbstractAnnotationCurve::AbstractAnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties) :
	AbstractAnnotationLine(startPosition, properties),
	// Straight: the handles of the ends lie on the ends themselves.
	mNodes{Node{QPointF(0, 0), QPointF(0, 0), QPointF(0, 0)}, Node{QPointF(1, 0), QPointF(1, 0), QPointF(1, 0)}}
{
}

AbstractAnnotationCurve::AbstractAnnotationCurve(const AbstractAnnotationCurve &other) :
	AbstractAnnotationLine(other),
	mNodes(other.mNodes)
{
}

QPointF AbstractAnnotationCurve::toPoint(const QPointF &relative) const
{
	const QPointF start = mLine->p1();
	const QPointF chord = mLine->p2() - start;
	const QPointF left(-chord.y(), chord.x()); // as long as the chord
	return start + relative.x() * chord + relative.y() * left;
}

std::optional<QPointF> AbstractAnnotationCurve::toRelative(const QPointF &point) const
{
	const QPointF chord = mLine->p2() - mLine->p1();
	const qreal length2 = dot(chord, chord);
	if (qFuzzyIsNull(length2)) {
		return std::nullopt; // no line to bend yet
	}
	const QPointF offset = point - mLine->p1();
	return QPointF(dot(offset, chord) / length2, dot(offset, QPointF(-chord.y(), chord.x())) / length2);
}

int AbstractAnnotationCurve::bendCount() const
{
	return int(mNodes.size()) - 2;
}

AbstractAnnotationCurve::Nodes AbstractAnnotationCurve::bends() const
{
	return mNodes;
}

void AbstractAnnotationCurve::setBends(const Nodes &nodes)
{
	if (nodes.size() < 2) {
		return;
	}
	prepareGeometryChange();
	mNodes = nodes;
	updateShape();
}

std::optional<AbstractAnnotationCurve::Nodes> AbstractAnnotationCurve::bendsWithPointAt(const QPointF &pos, qreal tolerance) const
{
	const std::optional<QPointF> target = toRelative(pos);
	if (!target) {
		return std::nullopt;
	}
	// In relative terms: the mapping to the scene only turns and scales, so
	// distances compare alike and the split is the same.
	const auto segmentAt = [this](int i, qreal t) {
		return cubicAt(mNodes[i].point, mNodes[i].out, mNodes[i + 1].in, mNodes[i + 1].point, t);
	};
	int nearestSegment = -1;
	qreal nearestT = 0;
	qreal nearest = std::numeric_limits<qreal>::max();
	for (int i = 0; i + 1 < mNodes.size(); ++i) {
		for (int step = 0; step <= kSamples; ++step) {
			const qreal t = qreal(step) / kSamples;
			const qreal distance = QLineF(segmentAt(i, t), *target).length();
			if (distance < nearest) {
				nearest = distance;
				nearestSegment = i;
				nearestT = t;
			}
		}
	}
	if (nearestSegment < 0) {
		return std::nullopt;
	}
	// Narrowed down between the neighbouring steps.
	qreal low = std::max(0.0, nearestT - 1.0 / kSamples);
	qreal high = std::min(1.0, nearestT + 1.0 / kSamples);
	for (int i = 0; i < kRefinements; ++i) {
		const qreal a = low + (high - low) / 3;
		const qreal b = high - (high - low) / 3;
		if (QLineF(segmentAt(nearestSegment, a), *target).length() < QLineF(segmentAt(nearestSegment, b), *target).length()) {
			high = b;
		} else {
			low = a;
		}
	}
	const qreal t = (low + high) / 2;
	const QPointF onCurve = toPoint(segmentAt(nearestSegment, t));
	if (QLineF(onCurve, pos).length() > tolerance || t <= 0 || t >= 1) {
		return std::nullopt; // off the curve, or on a node already
	}

	// De Casteljau: the segment split at t into two that together run
	// exactly where it ran.
	Nodes nodes = mNodes;
	Node &before = nodes[nearestSegment];
	Node &after = nodes[nearestSegment + 1];
	const QPointF a = lerp(before.point, before.out, t);
	const QPointF b = lerp(before.out, after.in, t);
	const QPointF c = lerp(after.in, after.point, t);
	const QPointF d = lerp(a, b, t);
	const QPointF e = lerp(b, c, t);
	before.out = a;
	after.in = c;
	nodes.insert(nearestSegment + 1, Node{lerp(d, e, t), d, e});
	return nodes;
}

std::optional<AbstractAnnotationCurve::Nodes> AbstractAnnotationCurve::bendsWithoutPoint(int index) const
{
	const int node = index - 1;
	if (node < 1 || node >= mNodes.size() - 1) {
		return std::nullopt;
	}
	Nodes nodes = mNodes;
	nodes.removeAt(node);
	return nodes;
}

void AbstractAnnotationCurve::setPointAt(const QPointF &point, int index, bool snapToAngle)
{
	if (index < 2) {
		// The ends: the nodes keep their places relative to them.
		AbstractAnnotationLine::setPointAt(point, index, snapToAngle);
		return;
	}
	const int node = index - 1;
	const std::optional<QPointF> relative = toRelative(point);
	if (node >= mNodes.size() - 1 || !relative) {
		return;
	}
	// The point takes its handles along.
	prepareGeometryChange();
	const QPointF delta = *relative - mNodes[node].point;
	mNodes[node].point += delta;
	mNodes[node].in += delta;
	mNodes[node].out += delta;
	updateShape();
}

QPointF AbstractAnnotationCurve::pointAt(int index) const
{
	if (index < 2) {
		return AbstractAnnotationLine::pointAt(index);
	}
	const int node = std::clamp(index - 1, 0, int(mNodes.size()) - 1);
	return toPoint(mNodes[node].point);
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
	// In the scene: each node and its handles.
	QList<QPointF> points;
	QList<QPointF> ins;
	QList<QPointF> outs;
	for (const Node &node : mNodes) {
		points.append(toPoint(node.point));
		ins.append(toPoint(node.in));
		outs.append(toPoint(node.out));
	}
	const int last = int(points.size()) - 1;
	const auto heads = properties()->arrowHeads();

	// A head at node `tip`, pointing along the curve: away from the first of
	// `towards` that is not on the tip. The shaft then stops a little short.
	mHeads = QPainterPath();
	const auto addHead = [this, &points](int tip, const QList<QPointF> &towards) {
		QLineF tangent(points[tip], points[tip]);
		for (const QPointF &inner : towards) {
			tangent = QLineF(inner, points[tip]);
			if (tangent.length() >= 1) {
				break;
			}
		}
		auto head = AnnotationShapeCreator::createArrowHead(properties()->width() / 2);
		head = AnnotationShapeCreator::translate(head, points[tip], -tangent.angle());
		mHeads.addPolygon(head);
		mHeads.closeSubpath();

		QLineF shortened(points[tip], tangent.p1());
		shortened.setLength(std::min(kShaftShortening, tangent.length()));
		points[tip] = shortened.p2();
	};
	if (heads == ArrowHeads::End || heads == ArrowHeads::Both) {
		addHead(last, {ins[last], outs[last - 1], points[last - 1]});
	}
	if (heads == ArrowHeads::Start || heads == ArrowHeads::Both) {
		addHead(0, {outs[0], ins[1], points[1]});
	}

	mCurve = QPainterPath(points.first());
	for (int i = 0; i < last; ++i) {
		mCurve.cubicTo(outs[i], ins[i + 1], points[i + 1]);
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
