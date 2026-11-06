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

#ifndef KIMAGEANNOTATOR_CURVEBENDSCOMMAND_H
#define KIMAGEANNOTATOR_CURVEBENDSCOMMAND_H

#include <QUndoCommand>

#include "src/annotations/items/AbstractAnnotationCurve.h"

namespace kImageAnnotator {

// A bend point added to a curve or removed from it: its nodes before
// and after.
class CurveBendsCommand : public QUndoCommand
{
public:
	CurveBendsCommand(AbstractAnnotationCurve *curve, const AbstractAnnotationCurve::Nodes &newBends);
	~CurveBendsCommand() override = default;
	void undo() override;
	void redo() override;

private:
	AbstractAnnotationCurve *mCurve;
	AbstractAnnotationCurve::Nodes mOldBends;
	AbstractAnnotationCurve::Nodes mNewBends;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_CURVEBENDSCOMMAND_H
