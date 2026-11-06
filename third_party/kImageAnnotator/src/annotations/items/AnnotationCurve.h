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

#ifndef KIMAGEANNOTATOR_ANNOTATIONCURVE_H
#define KIMAGEANNOTATOR_ANNOTATIONCURVE_H

#include "AbstractAnnotationCurve.h"

namespace kImageAnnotator {

// A line, straight or bent, with arrow heads where its properties say.
class AnnotationCurve : public AbstractAnnotationCurve
{
public:
	AnnotationCurve(const QPointF &startPosition, const PropertiesPtr &properties);
	AnnotationCurve(const AnnotationCurve &other);
	~AnnotationCurve() override = default;
	Tools toolType() const override;
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_ANNOTATIONCURVE_H
