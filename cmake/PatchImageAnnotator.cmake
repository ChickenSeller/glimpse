# Run by FetchContent in the kImageAnnotator source directory. The one change
# Glimpse makes to kImageAnnotator (LGPL-3.0, see THIRD_PARTY_NOTICES.md), to
# how it paints the canvas and the view around it: the whole view gets the
# same checkerboard as Glimpse's capture window (8 x 8 px cells, white and
# light gray, aligned to the canvas, fixed to the screen at any zoom) instead
# of a white view and kImageAnnotator's own 10 px checkerboard behind a
# transparent canvas; and a thin frame just outside the canvas showing exactly
# the area that is saved, while the view's "glimpseFramed" property is set
# (Glimpse sets it while the pointer is over the canvas, which it finds in
# the view's "glimpseCanvas" property, in view coordinates).
set(_file src/annotations/misc/CanvasPainter.cpp)
file(READ ${_file} content)

string(FIND "${content}" "namespace kImageAnnotator {" _start)
if(_start EQUAL -1)
    message(FATAL_ERROR "PatchImageAnnotator: unexpected ${_file}")
endif()
string(SUBSTRING "${content}" 0 ${_start} _header)
# Rewriting an already patched file gives the same result.
string(REPLACE "#include <QWidget>

" "" _header "${_header}")

file(WRITE ${_file} "${_header}#include <QWidget>

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
		view->setProperty(\"glimpseCanvas\", viewRect);
	if (view && view->property(\"glimpseFramed\").toBool()) {
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
")
