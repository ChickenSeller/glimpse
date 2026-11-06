/*
 * Copyright (C) 2020 Damir Porobic <damir.porobic@gmx.com>
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

#include "CanvasPainter.h"

#include <QWidget>

namespace kImageAnnotator {

// Glimpse: the checkerboard of Glimpse's capture window, 8 x 8 logical
// pixels per cell in white and light gray, aligned to screen pixels and the
// same size at every zoom level.

CanvasPainter::CanvasPainter() :
	mCanvasBackground(new QImage)
{
}

CanvasPainter::~CanvasPainter()
{
	delete mCanvasBackground;
}

void CanvasPainter::paint(QPainter *painter, const QRectF &rect, const QColor &color)
{
	const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
	const int cell = qMax(1, qRound(8 * dpr));
	if (mCanvasBackground->width() != 2 * cell) {
		*mCanvasBackground = QImage(2 * cell, 2 * cell, QImage::Format_ARGB32_Premultiplied);
		createTiledBackground();
		mCanvasBackground->setDevicePixelRatio(dpr);
	}

	// In view coordinates rather than the scene's, so zooming leaves the cells
	// alone. Over the whole view: what is saved is shown by the frame below.
	const QRectF viewRect = painter->transform().mapRect(rect);
	const auto view = dynamic_cast<QWidget *>(painter->device());
	painter->save();
	painter->resetTransform();
	painter->setPen(Qt::NoPen);
	painter->setBrush(*mCanvasBackground);
	painter->setBrushOrigin(viewRect.topLeft());
	painter->drawRect(view ? QRectF(view->rect()) : viewRect);

	// The frame: one screen pixel wide, just outside the canvas.
	if (view)
		view->setProperty("glimpseCanvas", viewRect);
	if (view && view->property("glimpseFramed").toBool()) {
		painter->setWorldTransform(QTransform::fromScale(1.0 / dpr, 1.0 / dpr));
		const QRectF native(viewRect.topLeft() * dpr, viewRect.size() * dpr);
		painter->setPen(QPen(QColor(0x2d, 0x9c, 0xff), 1));
		painter->setBrush(Qt::NoBrush);
		painter->drawRect(QRectF(native.toRect()).adjusted(-0.5, -0.5, 0.5, 0.5));
	}
	painter->restore();

	painter->setPen(Qt::NoPen);
	painter->setBrush(color);
	painter->drawRect(rect);
}

void CanvasPainter::createTiledBackground()
{
	const int cell = mCanvasBackground->width() / 2;
	mCanvasBackground->fill(Qt::white);
	QPainter painter(mCanvasBackground);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0xcc, 0xcc, 0xcc));
	painter.drawRect(0, 0, cell, cell);
	painter.drawRect(cell, cell, cell, cell);
}

} // namespace kImageAnnotator
