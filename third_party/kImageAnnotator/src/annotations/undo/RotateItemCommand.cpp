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

#include "RotateItemCommand.h"

#include <QTransform>

namespace kImageAnnotator {

RotateItemCommand::RotateItemCommand(AbstractAnnotationItem *item, const QPointF &center, qreal angle) :
	mItem(item),
	mOld{item->rotation(), item->transformOriginPoint(), item->pos()}
{
	// Turning about `center` instead of the old origin, at the old angle, must
	// leave the item where it is: the position makes up the difference.
	QTransform turn;
	turn.rotate(mOld.angle);
	const QPointF shift = mOld.origin - center;
	mNew = State{angle, center, mOld.position + shift - turn.map(shift)};
}

void RotateItemCommand::undo()
{
	apply(mOld);
}

void RotateItemCommand::redo()
{
	apply(mNew);
}

bool RotateItemCommand::mergeWith(const QUndoCommand *command)
{
	const auto other = dynamic_cast<const RotateItemCommand *>(command);
	if (other == nullptr || other->mItem != mItem) {
		return false;
	}
	mNew = other->mNew;
	return true;
}

int RotateItemCommand::id() const
{
	return Id;
}

void RotateItemCommand::apply(const State &state) const
{
	mItem->setTransformOriginPoint(state.origin);
	mItem->setRotation(state.angle);
	mItem->setPos(state.position);
}

} // namespace kImageAnnotator
