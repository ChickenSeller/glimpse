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

#ifndef KIMAGEANNOTATOR_ARROWHEADSPICKER_H
#define KIMAGEANNOTATOR_ARROWHEADSPICKER_H

#include <QLabel>

#include "SettingsPickerWidget.h"
#include "src/widgets/menuButtons/ListMenuToolButton.h"
#include "src/common/enum/ArrowHeads.h"
#include "src/common/helper/IconLoader.h"
#include "src/common/provider/ScaledSizeProvider.h"

namespace kImageAnnotator {

// Where a line has arrow heads: none, at its end, at its start or at both.
class ArrowHeadsPicker : public SettingsPickerWidget
{
	Q_OBJECT
public:
	explicit ArrowHeadsPicker(QWidget *parent);
	~ArrowHeadsPicker() override = default;
	void setArrowHeads(ArrowHeads arrowHeads);
	ArrowHeads arrowHeads() const;

signals:
	void arrowHeadsSelected(ArrowHeads arrowHeads) const;

protected:
	QWidget *expandingWidget() override;

private:
	QHBoxLayout *mLayout;
	QLabel *mLabel;
	ListMenuToolButton *mToolButton;

	void insertItem(ArrowHeads arrowHeads, const QString &iconName, const QString &text);
};

} // namespace kImageAnnotator

#endif // KIMAGEANNOTATOR_ARROWHEADSPICKER_H
