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

// A line that bends: a chain of cubic Bezier curves through its nodes, each
// node with a handle on either side, as in vector drawing programs. Drawn
// straight; a bend point added on it splits the curve there exactly, so the
// shape stays as it was until the point is dragged, and a dragged point
// takes its handles along. Arrow heads go where its properties say: at the
// end, the start, both or neither.
//
// Points by index: 0 the start, 1 the end, 2 and up the bend points from the
// start on, then, while a bend point is active (the one last taken hold of or
// added), its two handles: in, then out. Dragging a handle keeps the other
// in line with it (a smooth node, which stays so), unless the node is a
// corner already or the drag holds Ctrl, which moves the one handle alone.
// Nodes are kept relative to the two ends, so they follow them when they
// are moved, and the whole item when it is moved or scaled.
class AbstractAnnotationCurve : public AbstractAnnotationLine
{
public:
	// A point of the curve and its handles; relative to the two ends: a share
	// of the way from the start to the end, and a share of that length to its
	// left.
	struct Node {
		QPointF point;
		QPointF in;  // towards the node before
		QPointF out; // towards the node after
	};
	using Nodes = QList<Node>;

	AbstractAnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties);
	AbstractAnnotationCurve(const AbstractAnnotationCurve &other);
	~AbstractAnnotationCurve() override = default;
	void setPointAt(const QPointF &point, int index, bool snapToAngle) override;
	QPointF pointAt(int index) const override;
	bool requiresSelectionAfterCreation() const override;
	void setProperties(const PropertiesPtr &properties) override;

	int bendCount() const;
	// All points by index: the ends, the bend points, the active one's handles.
	int pointCount() const;
	// Index of the first handle point; meaningful while hasActiveNode().
	int firstHandleIndex() const;
	bool hasActiveNode() const;
	// The active bend point's index.
	int activePointIndex() const { return mActiveNode + 1; }
	// The point taken hold of: a bend point becomes the active one, an end
	// leaves none, a handle changes nothing.
	void takeHoldOf(int index);
	// All nodes, ends included, to restore them.
	Nodes bends() const;
	void setBends(const Nodes &nodes);
	// The nodes with a bend point added where the curve passes nearest
	// `pos`, if within `tolerance` of it; the shape stays the same.
	std::optional<Nodes> bendsWithPointAt(const QPointF &pos, qreal tolerance) const;
	// The nodes without the bend point `index` (2 and up).
	std::optional<Nodes> bendsWithoutPoint(int index) const;

protected:
	void updateShape() override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
	Nodes mNodes; // the start, the bend points, the end
	int mActiveNode = -1; // a bend point's node, or -1
	QPainterPath mCurve; // stroked only
	QPainterPath mHeads; // stroked and filled

	QPointF toPoint(const QPointF &relative) const;
	std::optional<QPointF> toRelative(const QPointF &point) const;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_ABSTRACTANNOTATIONCURVE_H
