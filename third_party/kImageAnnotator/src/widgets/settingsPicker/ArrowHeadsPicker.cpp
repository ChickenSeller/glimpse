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

#include "ArrowHeadsPicker.h"

namespace kImageAnnotator {

ArrowHeadsPicker::ArrowHeadsPicker(QWidget *parent) :
	SettingsPickerWidget(parent),
	mLayout(new QHBoxLayout(this)),
	mLabel(new QLabel(this)),
	mToolButton(new ListMenuToolButton(this))
{
	mLayout->setContentsMargins(0, 0, 0, 0);

	auto icon = IconLoader::load(QLatin1String("arrow.svg"));
	mLabel->setPixmap(icon.pixmap(ScaledSizeProvider::settingsWidgetIconSize()));
	mLabel->setToolTip(tr("Arrow Heads"));

	insertItem(ArrowHeads::None, QLatin1String("arrowHeads_none.svg"), tr("No Arrow Heads"));
	insertItem(ArrowHeads::End, QLatin1String("arrowHeads_end.svg"), tr("At the End"));
	insertItem(ArrowHeads::Start, QLatin1String("arrowHeads_start.svg"), tr("At the Start"));
	insertItem(ArrowHeads::Both, QLatin1String("arrowHeads_both.svg"), tr("At Both Ends"));

	mToolButton->setFocusPolicy(Qt::NoFocus);
	connect(mToolButton, &ListMenuToolButton::selectionChanged, this, [this] {
		emit arrowHeadsSelected(arrowHeads());
	});

	mLayout->addWidget(mLabel);
	mLayout->addWidget(mToolButton);
	mLayout->setAlignment(Qt::AlignLeft);
	setLayout(mLayout);
}

void ArrowHeadsPicker::setArrowHeads(ArrowHeads arrowHeads)
{
	mToolButton->setCurrentData(static_cast<int>(arrowHeads));
}

ArrowHeads ArrowHeadsPicker::arrowHeads() const
{
	return static_cast<ArrowHeads>(mToolButton->currentData().toInt());
}

QWidget *ArrowHeadsPicker::expandingWidget()
{
	return mToolButton;
}

void ArrowHeadsPicker::insertItem(ArrowHeads arrowHeads, const QString &iconName, const QString &text)
{
	mToolButton->addItem(IconLoader::load(iconName), text, static_cast<int>(arrowHeads));
}

} // namespace kImageAnnotator
