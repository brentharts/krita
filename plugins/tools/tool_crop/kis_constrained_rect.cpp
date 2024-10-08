/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_constrained_rect.h"

#include <cmath>
#include "kis_debug.h"
#include "kis_algebra_2d.h"


KisConstrainedRect::KisConstrainedRect()
{
}

KisConstrainedRect::~KisConstrainedRect()
{
}

void KisConstrainedRect::setRectInitial(const QRect &rect)
{
    m_rect = rect;
    if (!ratioLocked()) {
        storeRatioSafe(m_rect.size());
    }
    Q_EMIT sigValuesChanged();
}

void KisConstrainedRect::setCropRect(const QRect &cropRect)
{
    m_cropRect = cropRect;
}

bool KisConstrainedRect::centered() const {
    return m_centered;
}
void KisConstrainedRect::setCentered(bool value) {
    m_centered = value;
}

bool KisConstrainedRect::canGrow() const {
    return m_canGrow;
}
void KisConstrainedRect::setCanGrow(bool value) {
    m_canGrow = value;
}

QRect KisConstrainedRect::rect() const {
    return m_rect.normalized();
}

qreal KisConstrainedRect::ratio() const {
    return qAbs(m_ratio);
}

void KisConstrainedRect::moveHandle(HandleType handle, const QPoint &offset, const QRect &oldRect)
{
    const QSize oldSize = oldRect.size();
    QSize newSize = oldSize;
    QPoint newOffset = oldRect.topLeft();

    int xSizeCoeff = 1;
    int ySizeCoeff = 1;

    qreal xOffsetFromSizeChange = 1.0;
    qreal yOffsetFromSizeChange = 1.0;

    int baseSizeCoeff = 1;

    bool useMoveOnly = false;

    switch (handle) {
    case UpperLeft:
        xSizeCoeff = -1;
        ySizeCoeff = -1;
        xOffsetFromSizeChange = -1.0;
        yOffsetFromSizeChange = -1.0;
        break;
    case UpperRight:
        xSizeCoeff =  1;
        ySizeCoeff = -1;
        xOffsetFromSizeChange =  0.0;
        yOffsetFromSizeChange = -1.0;
        break;
    case Creation:
        baseSizeCoeff = 0;
        Q_FALLTHROUGH();
    case LowerRight:
        xSizeCoeff =  1;
        ySizeCoeff =  1;
        xOffsetFromSizeChange =  0.0;
        yOffsetFromSizeChange =  0.0;
        break;
    case LowerLeft:
        xSizeCoeff = -1;
        ySizeCoeff =  1;
        xOffsetFromSizeChange = -1.0;
        yOffsetFromSizeChange =  0.0;
        break;
    case Upper:
        xSizeCoeff =  0;
        ySizeCoeff = -1;
        xOffsetFromSizeChange = -0.5;
        yOffsetFromSizeChange = -1.0;
        break;
    case Right:
        xSizeCoeff =  1;
        ySizeCoeff =  0;
        xOffsetFromSizeChange =  0.0;
        yOffsetFromSizeChange = -0.5;
        break;
    case Lower:
        xSizeCoeff =  0;
        ySizeCoeff =  1;
        xOffsetFromSizeChange = -0.5;
        yOffsetFromSizeChange =  0.0;
        break;
    case Left:
        xSizeCoeff = -1;
        ySizeCoeff =  0;
        xOffsetFromSizeChange = -1.0;
        yOffsetFromSizeChange = -0.5;
        break;
    case Inside:
        useMoveOnly = true;
        break;
    case None: // should never happen
        break;
    }

    if (!useMoveOnly) {
        const int centeringSizeCoeff = m_centered ? 2 : 1;
        if (m_centered) {
            xOffsetFromSizeChange = -0.5;
            yOffsetFromSizeChange = -0.5;
        }


        QSize sizeDiff(offset.x() * xSizeCoeff * centeringSizeCoeff,
                       offset.y() * ySizeCoeff * centeringSizeCoeff);

        QSize tempSize = baseSizeCoeff * oldSize + sizeDiff;
        bool widthPreferable = qAbs(tempSize.width()) > qAbs(tempSize.height() * m_ratio);

        if (ratioLocked()) {
            if ((widthPreferable && xSizeCoeff != 0) || ySizeCoeff == 0) {
                newSize.setWidth(tempSize.width());
                newSize.setHeight(heightFromWidthUnsignedRatio(newSize.width(), m_ratio, tempSize.height()));
            } else if ((!widthPreferable && ySizeCoeff != 0) || xSizeCoeff == 0) {
                newSize.setHeight(tempSize.height());
                newSize.setWidth(widthFromHeightUnsignedRatio(newSize.height(), m_ratio, tempSize.width()));
            }

            // see https://bugs.kde.org/show_bug.cgi?id=432036
            if (!m_canGrow && qAbs(newSize.width()) > m_cropRect.width()) {
                newSize.setWidth(m_cropRect.width());
                newSize.setHeight(heightFromWidthUnsignedRatio(newSize.width(), m_ratio, newSize.height()));
            }
            if (!m_canGrow && qAbs(newSize.height()) > m_cropRect.height()) {
                newSize.setHeight(m_cropRect.height());
                newSize.setWidth(widthFromHeightUnsignedRatio(newSize.height(), m_ratio, newSize.width()));
            }
        } else if (widthLocked() && heightLocked()) {
            newSize.setWidth(KisAlgebra2D::copysign(newSize.width(), tempSize.width()));
            newSize.setHeight(KisAlgebra2D::copysign(newSize.height(), tempSize.height()));
        } else if (widthLocked()) {
            newSize.setWidth(KisAlgebra2D::copysign(newSize.width(), tempSize.width()));
            newSize.setHeight(tempSize.height());
            storeRatioSafe(newSize);
        } else if (heightLocked()) {
            newSize.setHeight(KisAlgebra2D::copysign(newSize.height(), tempSize.height()));
            newSize.setWidth(tempSize.width());
            storeRatioSafe(newSize);
        } else {
            newSize = baseSizeCoeff * oldSize + sizeDiff;
            storeRatioSafe(newSize);
        }

        QSize realSizeDiff = newSize - baseSizeCoeff * oldSize;
        QPoint offsetDiff(realSizeDiff.width() * xOffsetFromSizeChange,
                          realSizeDiff.height() * yOffsetFromSizeChange);

        newOffset = oldRect.topLeft() + offsetDiff;
    } else {
        newOffset = oldRect.topLeft() + offset;
    }

    QPoint prevOffset = newOffset;

    if (!m_canGrow) {
        if (newOffset.x() + newSize.width() > m_cropRect.width()) {
            newOffset.setX(m_cropRect.width() - newSize.width());
        }

        if (newOffset.y() + newSize.height() > m_cropRect.height()) {
            newOffset.setY(m_cropRect.height() - newSize.height());
        }
        if (newOffset.x() < m_cropRect.x()) {
            newOffset.setX(m_cropRect.x());
        }
        if (newOffset.y() < m_cropRect.y()) {
            newOffset.setY(m_cropRect.y());
        }
    }

    if (!m_ratioLocked && !useMoveOnly) {
        newOffset = prevOffset;
    }

    m_rect = QRect(newOffset, newSize);

    if (!m_canGrow) {
        m_rect &= m_cropRect;

        if (ratioLocked() && m_rect.height()) {
            qreal newRatio = m_rect.width() / (qreal)(m_rect.height());

            if (newRatio > m_ratio) {
                m_rect.setWidth(widthFromHeightUnsignedRatio(m_rect.height(), m_ratio, m_rect.width()));
            } else {
                m_rect.setHeight(heightFromWidthUnsignedRatio(m_rect.width(), m_ratio, m_rect.height()));
            }
        }
    }

    Q_EMIT sigValuesChanged();
}

QPointF KisConstrainedRect::handleSnapPoint(HandleType handle, const QPointF &cursorPos)
{
    QPointF snapPoint = cursorPos;

    switch (handle) {
    case UpperLeft:
        snapPoint = m_rect.topLeft();
        break;
    case UpperRight:
        snapPoint = m_rect.topRight() + QPointF(1, 0);
        break;
    case Creation:
        break;
    case LowerRight:
        snapPoint = m_rect.bottomRight() + QPointF(1, 1);
        break;
    case LowerLeft:
        snapPoint = m_rect.bottomLeft() + QPointF(0, 1);
        break;
    case Upper:
        snapPoint.ry() = m_rect.y();
        break;
    case Right:
        snapPoint.rx() = m_rect.right() + 1;
        break;
    case Lower:
        snapPoint.ry() = m_rect.bottom() + 1;
        break;
    case Left:
        snapPoint.rx() = m_rect.x();
        break;
    case Inside:
        break;
    case None: // should never happen
        break;
    }

    return snapPoint;
}

void KisConstrainedRect::normalize()
{
    setRectInitial(m_rect.normalized());
}

void KisConstrainedRect::setOffset(const QPoint &offset)
{
    QRect newRect = m_rect;
    newRect.moveTo(offset);

    if (!m_canGrow) {
        newRect &= m_cropRect;
    }

    if (!newRect.isEmpty()) {
        m_rect = newRect;
    }

    Q_EMIT sigValuesChanged();
}

void KisConstrainedRect::setRatio(qreal value) {
    KIS_ASSERT_RECOVER_RETURN(value >= 0);

    const qreal eps = 1e-7;
    const qreal invEps = 1.0 / eps;

    if (value < eps || value > invEps) {
        Q_EMIT sigValuesChanged();
        return;
    }

    const QSize oldSize = m_rect.size();
    QSize newSize = oldSize;

    if (widthLocked() && heightLocked()) {
        setHeightLocked(false);
    }

    m_ratio = value;

    if (!widthLocked() && !heightLocked()) {
        int area = oldSize.width() * oldSize.height();
        newSize.setWidth(qRound(std::sqrt(area * m_ratio)));
        newSize.setHeight(qRound(newSize.width() / m_ratio));
    } else if (widthLocked()) {
        newSize.setHeight(newSize.width() / m_ratio);
    } else if (heightLocked()) {
        newSize.setWidth(newSize.height() * m_ratio);
    }

    assignNewSize(newSize);
}

void KisConstrainedRect::setWidth(int value)
{
    KIS_ASSERT_RECOVER_RETURN(value >= 0);

    const QSize oldSize = m_rect.size();
    QSize newSize = oldSize;

    if (ratioLocked()) {
        newSize.setWidth(value);
        newSize.setHeight(newSize.width() / m_ratio);
    } else {
        newSize.setWidth(value);
        storeRatioSafe(newSize);
    }

    assignNewSize(newSize);
}

void KisConstrainedRect::setHeight(int value)
{
    KIS_ASSERT_RECOVER_RETURN(value >= 0);

    const QSize oldSize = m_rect.size();
    QSize newSize = oldSize;

    if (ratioLocked()) {
        newSize.setHeight(value);
        newSize.setWidth(newSize.height() * m_ratio);
    } else {
        newSize.setHeight(value);
        storeRatioSafe(newSize);
    }

    assignNewSize(newSize);
}

void KisConstrainedRect::assignNewSize(const QSize &newSize)
{
    if (!m_centered) {
        m_rect.setSize(newSize);
    } else {
        QSize sizeDiff = newSize - m_rect.size();
        m_rect.translate(-qRound(sizeDiff.width() / 2.0), -qRound(sizeDiff.height() / 2.0));
        m_rect.setSize(newSize);
    }

    if (!m_canGrow) {
        m_rect &= m_cropRect;
    }

    Q_EMIT sigValuesChanged();
}

void KisConstrainedRect::storeRatioSafe(const QSize &newSize)
{
    m_ratio = qAbs(qreal(newSize.width()) / newSize.height());
}

int KisConstrainedRect::widthFromHeightUnsignedRatio(int height, qreal ratio, int oldWidth) const
{
    int newWidth = qRound(height * ratio);
    return KisAlgebra2D::copysign(newWidth, oldWidth);
}

int KisConstrainedRect::heightFromWidthUnsignedRatio(int width, qreal ratio, int oldHeight) const
{
    int newHeight = qRound(width / ratio);
    return KisAlgebra2D::copysign(newHeight, oldHeight);
}

bool KisConstrainedRect::widthLocked() const {
    return m_widthLocked;
}
bool KisConstrainedRect::heightLocked() const {
    return m_heightLocked;
}
bool KisConstrainedRect::ratioLocked() const {
    return m_ratioLocked;
}

void KisConstrainedRect::setWidthLocked(bool value) {
    m_widthLocked = value;
    m_ratioLocked &= !(m_widthLocked || m_heightLocked);

    Q_EMIT sigLockValuesChanged();
}

void KisConstrainedRect::setHeightLocked(bool value) {
    m_heightLocked = value;
    m_ratioLocked &= !(m_widthLocked || m_heightLocked);

    Q_EMIT sigLockValuesChanged();
}

void KisConstrainedRect::setRatioLocked(bool value) {
    m_ratioLocked = value;

    m_widthLocked &= !m_ratioLocked;
    m_heightLocked &= !m_ratioLocked;

    Q_EMIT sigLockValuesChanged();
}

