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

#ifndef KIMAGEANNOTATOR_ARROWHEADS_H
#define KIMAGEANNOTATOR_ARROWHEADS_H

#include <QMetaType>

namespace kImageAnnotator {

// Where a line has arrow heads.
enum class ArrowHeads
{
	None,
	End,
	Start,
	Both
};

} // namespace kImageAnnotator

Q_DECLARE_METATYPE(kImageAnnotator::ArrowHeads);

#endif // KIMAGEANNOTATOR_ARROWHEADS_H
