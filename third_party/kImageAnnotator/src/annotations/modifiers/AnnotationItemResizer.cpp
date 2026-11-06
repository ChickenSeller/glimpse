/*
 * Copyright (C) 2018 Damir Porobic <damir.porobic@gmx.com>
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

#include "AnnotationItemResizer.h"

#include <QLineF>

#include <cmath>

namespace kImageAnnotator {

AnnotationItemResizer::AnnotationItemResizer(AbstractAnnotationItem *item, ZoomValueProvider *zoomValueProvider) :
	mAnnotationItem(item),
	mZoomValueProvider(zoomValueProvider),
	mCurrentHandle(-1)
{
    mResizeHandles = ResizeHandlesFactory::createResizeHandles(item, zoomValueProvider->zoomValue());
    connect(zoomValueProvider, &ZoomValueProvider::zoomValueChanged, this, &AnnotationItemResizer::applyZoomValue);
    prepareGeometryChange();
    followItem();
}

AnnotationItemResizer::~AnnotationItemResizer()
{
    mAnnotationItem = nullptr;
    delete mResizeHandles;
}

QRectF AnnotationItemResizer::boundingRect() const
{
    auto size = mResizeHandles->handleSize() / 2;
    auto rect = mAnnotationItem->boundingRect().normalized().adjusted(-size, -size, size, size);
    // A curve's handles may stick out of it; the turning handle does.
    const auto handles = mResizeHandles->handles();
    for (const auto &handle : handles) {
        rect |= handle;
    }
    const auto radius = rotateHandleRadius();
    rect |= QRectF(rotateHandleCenter() - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius));
    return rect;
}

// Above the middle of the item's top edge, in its coordinates.
QPointF AnnotationItemResizer::rotateHandleCenter() const
{
    const auto rect = mAnnotationItem->boundingRect().normalized();
    return { rect.center().x(), rect.top() - 24 / mZoomValueProvider->zoomValue() };
}

qreal AnnotationItemResizer::rotateHandleRadius() const
{
    return 5 / mZoomValueProvider->zoomValue();
}

void AnnotationItemResizer::grabHandle(const QPointF &scenePos)
{
    // Positions come in the scene; the handles are in the item's coordinates.
    const QPointF pos = mAnnotationItem->mapFromScene(scenePos);
    mRotating = false;
    if (QLineF(pos, rotateHandleCenter()).length() <= rotateHandleRadius() * 1.5) {
        mRotating = true;
        mRotationCenter = mAnnotationItem->boundingRect().normalized().center();
        mRotationCenterInScene = mAnnotationItem->mapToScene(mRotationCenter);
        mStartRotation = mAnnotationItem->rotation();
        mStartPointerAngle = QLineF(mRotationCenterInScene, scenePos).angle();
        mCurrentHandle = -1;
        return;
    }
    mCurrentHandle = mResizeHandles->indexOfHandleAt(pos);
    // A curve's point taken hold of shows its handles (or hides them).
    if (auto curve = dynamic_cast<AbstractAnnotationCurve *>(mAnnotationItem); curve != nullptr && mCurrentHandle != -1) {
        curve->takeHoldOf(mCurrentHandle);
        refresh();
    }
    if (mCurrentHandle != -1) {
        mClickOffset = pos - mResizeHandles->handle(mCurrentHandle).anchor();
    }
}

void AnnotationItemResizer::moveHandle(const QPointF &scenePos, bool isCtrlPressed)
{
    if (mRotating) {
        // QLineF measures angles counterclockwise; items turn clockwise.
        auto angle = mStartRotation - (QLineF(mRotationCenterInScene, scenePos).angle() - mStartPointerAngle);
        if (isCtrlPressed) {
            angle = std::round(angle / 15) * 15; // Shift or Ctrl: steps of 15°
        }
        angle = std::fmod(angle, 360.0);
        emit newCommand(new RotateItemCommand(mAnnotationItem, mRotationCenter, angle));
        return;
    }
    if (mCurrentHandle != -1) {
		const QPointF pos = mAnnotationItem->mapFromScene(scenePos);
		emit newCommand(new ResizeCommand(mAnnotationItem, mCurrentHandle, pos - mClickOffset, isCtrlPressed));
    }
}

void AnnotationItemResizer::releaseHandle()
{
    mCurrentHandle = -1;
    mRotating = false;
}

bool AnnotationItemResizer::isResizing() const
{
    return mCurrentHandle != -1 || mRotating;
}

void AnnotationItemResizer::refresh()
{
    prepareGeometryChange();
    mResizeHandles->update();
    followItem();
}

// Turned and placed with the item, since the handles are in its
// coordinates: its scene transform, less the parent's. Joining a group moves
// the translation into the position (to keep the place), so the position is
// reset here, or the translation would count twice.
void AnnotationItemResizer::followItem()
{
    const auto parent = parentItem();
    setPos(0, 0);
    setTransform(mAnnotationItem->sceneTransform() * (parent != nullptr ? parent->sceneTransform().inverted() : QTransform()));
}

bool AnnotationItemResizer::isItemVisible() const
{
	return mAnnotationItem != nullptr && mAnnotationItem->isVisible();
}

Qt::CursorShape AnnotationItemResizer::cursorForPos(const QPointF &scenePos)
{
    const QPointF pos = mAnnotationItem->mapFromScene(scenePos);
    if (QLineF(pos, rotateHandleCenter()).length() <= rotateHandleRadius() * 1.5) {
        return Qt::PointingHandCursor;
    }
    return mResizeHandles->cursorForPos(pos);
}

// A curve's handles, and the lines to them, show the shape being made.
bool AnnotationItemResizer::staysShownWhileDragged() const
{
    return dynamic_cast<AbstractAnnotationCurve *>(mAnnotationItem) != nullptr && !mRotating;
}

Qt::CursorShape AnnotationItemResizer::cursorForCurrentHandle()
{
    if (mRotating) {
        return Qt::ClosedHandCursor;
    }
    return mResizeHandles->cursorForHandle(mCurrentHandle);
}

void AnnotationItemResizer::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    // The turning handle, on a stalk from the top edge.
    {
        const auto rect = mAnnotationItem->boundingRect().normalized();
        const auto center = rotateHandleCenter();
        const auto radius = rotateHandleRadius();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(Qt::gray, 1.0 / mZoomValueProvider->zoomValue()));
        painter->drawLine(QPointF(center.x(), rect.top()), center + QPointF(0, radius));
        painter->setPen(QPen(Qt::white, 1.0 / mZoomValueProvider->zoomValue()));
        painter->setBrush(QColor(0x2d, 0x9c, 0xff));
        painter->drawEllipse(center, radius, radius);
        painter->restore();
    }

    painter->setPen(QPen(Qt::white, 1.0 / mZoomValueProvider->zoomValue()));
    painter->setBrush(Qt::gray);
    auto handles = mResizeHandles->handles();
    // A curve's active point: a line to each of its handles, drawn round.
    auto curve = dynamic_cast<AbstractAnnotationCurve *>(mAnnotationItem);
    const int firstHandle = curve != nullptr && curve->hasActiveNode() ? curve->firstHandleIndex() : int(handles.size());
    if (firstHandle + 1 < handles.size()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(Qt::gray, 1.0 / mZoomValueProvider->zoomValue()));
        const QPointF node = curve->pointAt(curve->activePointIndex());
        painter->drawLine(node, curve->pointAt(firstHandle));
        painter->drawLine(node, curve->pointAt(firstHandle + 1));
        painter->restore();
    }
    for (int i = 0; i < handles.size(); ++i) {
        if (i >= firstHandle) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->drawEllipse(handles[i]);
            painter->restore();
        } else {
            painter->drawRect(handles[i]);
        }
    }
}

void AnnotationItemResizer::applyZoomValue(double value)
{
    mResizeHandles->applyZoomValue(value);
}

} // namespace kImageAnnotator
