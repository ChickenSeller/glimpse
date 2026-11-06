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

#ifndef KIMAGEANNOTATOR_ROTATEITEMCOMMAND_H
#define KIMAGEANNOTATOR_ROTATEITEMCOMMAND_H

#include <QUndoCommand>

#include "src/annotations/items/AbstractAnnotationItem.h"

namespace kImageAnnotator {

// An item turned about `center` (in its own coordinates) to `angle` degrees,
// clockwise. Its shape stays in its own coordinates; the item's transform
// (origin, rotation, position) does the turning, and is restored exactly.
class RotateItemCommand : public QUndoCommand
{
public:
	enum
	{
		Id = 1240
	};

	RotateItemCommand(AbstractAnnotationItem *item, const QPointF &center, qreal angle);
	~RotateItemCommand() override = default;
	void undo() override;
	void redo() override;
	bool mergeWith(const QUndoCommand *command) override;
	int id() const override;

private:
	struct State {
		qreal angle;
		QPointF origin;
		QPointF position;
	};
	AbstractAnnotationItem *mItem;
	State mOld;
	State mNew;

	void apply(const State &state) const;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_ROTATEITEMCOMMAND_H
