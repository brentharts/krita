/*
 *  kis_tool_transform.cc -- part of Krita
 *
 *  Copyright (c) 2004 Boudewijn Rempt <boud@valdyas.org>
 *  Copyright (c) 2005 Casper Boemann <cbr@boemann.dk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_tool_transform.h"

#include <math.h>

#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QObject>
#include <QLabel>
#include <QComboBox>
#include <QApplication>

#include <kis_debug.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <klocale.h>
#include <knuminput.h>

#include <KoPointerEvent.h>
#include <KoID.h>
#include <KoCanvasBase.h>
#include <KoViewConverter.h>
#include <KoSelection.h>
#include <KoCompositeOp.h>
#include <KoShapeManager.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>

#include <kis_global.h>
#include <canvas/kis_canvas2.h>
#include <kis_view2.h>
#include <kis_painter.h>
#include <kis_cursor.h>
#include <kis_image.h>
#include <kis_undo_adapter.h>
#include <kis_transaction.h>
#include <kis_selection.h>
#include <kis_filter_strategy.h>
#include <widgets/kis_cmb_idlist.h>
#include <kis_statusbar.h>
#include <kis_transform_worker.h>
#include <kis_pixel_selection.h>
#include <kis_shape_selection.h>
#include <kis_selection_manager.h>

#include <KoShapeTransformCommand.h>

#include "flake/kis_node_shape.h"
#include "flake/kis_layer_container_shape.h"
#include "flake/kis_shape_layer.h"
#include "kis_canvas_resource_provider.h"
#include "widgets/kis_progress_widget.h"

#define KISPAINTDEVICE_MOVE_DEPRECATED

namespace
{
class ApplyTransformCmdData : public KisSelectedTransactionData
{

public:
    ApplyTransformCmdData(KisToolTransform *tool, KisNodeSP node, ToolTransformArgs args, KisSelectionSP origSel, QPoint startPos, QPoint endPos, QImage *origImg, QImage *origSelectionImg);
    virtual ~ApplyTransformCmdData();

public:
    virtual void redo();
    virtual void undo();
    void transformArgs(ToolTransformArgs &args) const;
    KisSelectionSP origSelection(QPoint &startPos, QPoint &endPos) const;
	void origPreviews(QImage *&origImg, QImage *&origSelectionImg) const;
private:
	ToolTransformArgs m_args;
    KisToolTransform *m_tool;
    KisSelectionSP m_origSelection;
    QPoint m_originalTopLeft;
    QPoint m_originalBottomRight;
	QImage *m_origImg, *m_origSelectionImg;
};

ApplyTransformCmdData::ApplyTransformCmdData(KisToolTransform *tool, KisNodeSP node, ToolTransformArgs args, KisSelectionSP origSel, QPoint originalTopLeft, QPoint originalBottomRight, QImage *origImg, QImage *origSelectionImg)
        : KisSelectedTransactionData(i18n("Apply transformation"), node)
        , m_args(args)
        , m_tool(tool)
        , m_origSelection(origSel)
        , m_originalTopLeft(originalTopLeft)
        , m_originalBottomRight(originalBottomRight)
        , m_origImg(origImg)
        , m_origSelectionImg(origSelectionImg)
{
}

ApplyTransformCmdData::~ApplyTransformCmdData()
{
}

void ApplyTransformCmdData::transformArgs(ToolTransformArgs &args) const
{
	args = m_args;
}

KisSelectionSP ApplyTransformCmdData::origSelection(QPoint &originalTopLeft, QPoint &originalBottomRight) const
{
    originalTopLeft = m_originalTopLeft;
    originalBottomRight = m_originalBottomRight;
    return m_origSelection;
}

void ApplyTransformCmdData::origPreviews(QImage *&origImg, QImage *&origSelectionImg) const
{
	origImg = m_origImg;
	origSelectionImg = m_origSelectionImg;
}

void ApplyTransformCmdData::redo()
{
    KisSelectedTransactionData::redo();
}

void ApplyTransformCmdData::undo()
{
    KisSelectedTransactionData::undo();
}
}

class ApplyTransformCmd : public KisTransaction
{
public:
    ApplyTransformCmd(KisToolTransform *tool, KisNodeSP node,
							ToolTransformArgs args,
							KisSelectionSP origSel,
							QPoint originalTopLeft, QPoint originalBottomRight,
							QImage *origImg, QImage *origSelectionImg)
    {
        m_transactionData =
            new ApplyTransformCmdData(tool, node, args, origSel, originalTopLeft, originalBottomRight, origImg, origSelectionImg);
    }
};

namespace
{
class TransformCmd : public QUndoCommand
{
public:
    TransformCmd(KisToolTransform *tool, ToolTransformArgs args, KisSelectionSP origSel, QPoint startPos, QPoint endPos, QImage *origImg, QImage *origSelectionImg);
    virtual ~TransformCmd();

public:
    virtual void redo();
    virtual void undo();
    void transformArgs(ToolTransformArgs &args) const;
    KisSelectionSP origSelection(QPoint &startPos, QPoint &endPos) const;
	void origPreviews(QImage *&origImg, QImage *&origSelectionImg) const;
private:
	ToolTransformArgs m_args;
    KisToolTransform *m_tool;
    KisSelectionSP m_origSelection;
    QPoint m_originalTopLeft;
    QPoint m_originalBottomRight;
	QImage *m_origImg, *m_origSelectionImg;
};

TransformCmd::TransformCmd(KisToolTransform *tool, ToolTransformArgs args, KisSelectionSP origSel, QPoint startPos, QPoint endPos, QImage *origImg, QImage *origSelectionImg)
{
	m_args = args;
	m_tool = tool;
	m_origSelection = origSel;
	m_originalTopLeft = startPos;
	m_originalBottomRight = endPos;
	m_origImg = origImg;
	m_origSelectionImg = origSelectionImg;
}

TransformCmd::~TransformCmd()
{
}

void TransformCmd::transformArgs(ToolTransformArgs &args) const
{
	args = m_args;
}

KisSelectionSP TransformCmd::origSelection(QPoint &originalTopLeft, QPoint &originalBottomRight) const
{
    originalTopLeft = m_originalTopLeft;
    originalBottomRight = m_originalBottomRight;
    return m_origSelection;
}

void TransformCmd::origPreviews(QImage *&origImg, QImage *&origSelectionImg) const
{
	origImg = m_origImg;
	origSelectionImg = m_origSelectionImg;
}

void TransformCmd::redo()
{
}

void TransformCmd::undo()
{
}
}

//returns the smallest QRectF containing 4 points
static QRectF boundRect(QPointF P0, QPointF P1, QPointF P2, QPointF P3)
{
	QRectF res(P0, P0);
	QPointF P[] = {P1, P2, P3};

	for (int i = 0; i < 3; ++i) {
		if ( P[i].x() < res.left() )
			res.setLeft(P[i].x());
		else if ( P[i].x() > res.right() )
			res.setRight(P[i].x());

		if ( P[i].y() < res.top() )
			res.setTop(P[i].y());
		else if ( P[i].y() > res.bottom() )
			res.setBottom(P[i].y());
	}

	return res;
}

//returns a value in [0; 360[
static double radianToDegree(double rad)
{
	double piX2 = 2 * M_PI;

	if (rad < 0 || rad >= piX2) {
		rad = fmod(rad, piX2);
		if (rad < 0)
			rad += piX2;
	}

	return (rad * 360. / piX2);
}

//returns a value in [0; 2 * M_PI[
static double degreeToRadian(double degree)
{
	if (degree < 0. || degree >= 360.) {
		degree = fmod(degree, 360.);
		if (degree < 0)
			degree += 360.;
	}

	return (degree * M_PI / 180.);
}
KisToolTransform::KisToolTransform(KoCanvasBase * canvas)
        : KisTool(canvas, KisCursor::rotateCursor())
        , m_canvas(canvas)
{
    setObjectName("tool_transform");
    useCursor(KisCursor::selectCursor());
    m_selecting = false;
    m_originalTopLeft = QPoint(0, 0);
    m_originalBottomRight = QPoint(0, 0);
    m_previousTopLeft = QPoint(0, 0);
    m_previousBottomRight = QPoint(0, 0);
	m_viewerZ = 1000; //arbitrary value
    m_optWidget = 0;
    m_sizeCursors[0] = KisCursor::sizeHorCursor();
    m_sizeCursors[1] = KisCursor::sizeBDiagCursor();
    m_sizeCursors[2] = KisCursor::sizeVerCursor();
    m_sizeCursors[3] = KisCursor::sizeFDiagCursor();
    m_sizeCursors[4] = KisCursor::sizeHorCursor();
    m_sizeCursors[5] = KisCursor::sizeBDiagCursor();
    m_sizeCursors[6] = KisCursor::sizeVerCursor();
    m_sizeCursors[7] = KisCursor::sizeFDiagCursor();
	m_handleDir[0] = QPointF(1, 0);
	m_handleDir[1] = QPointF(1, -1);
	m_handleDir[2] = QPointF(0, -1);
	m_handleDir[3] = QPointF(-1, -1);
	m_handleDir[4] = QPointF(-1, 0);
	m_handleDir[5] = QPointF(-1, 1);
	m_handleDir[6] = QPointF(0, 1);
	m_handleDir[7] = QPointF(1, 1);
	m_handleDir[8] = QPointF(0, 0); //also add the center
    m_origDevice = 0;
    m_origSelection = 0;
	m_handleRadius = 8;
	m_rotationCenterRadius = 12;
	m_boxValueChanged = false;
	m_maxRadius = (m_handleRadius > m_rotationCenterRadius) ? m_handleRadius : m_rotationCenterRadius;
}

KisToolTransform::~KisToolTransform()
{
}

void KisToolTransform::storeArgs(ToolTransformArgs &args)
{
	args = m_currentArgs;
}

void KisToolTransform::restoreArgs(ToolTransformArgs args)
{
	m_currentArgs = args;
}

void KisToolTransform::updateCurrentOutline()
{
	KisImageWSP kisimage = image();

	if (kisimage) {
		QRectF rc = boundRect(m_topLeftProj, m_topRightProj, m_bottomRightProj, m_bottomLeftProj);
		rc = QRect(QPoint(rc.left() / kisimage->xRes(), rc.top() / kisimage->yRes()), QPoint(rc.right() / kisimage->xRes(), rc.bottom() / kisimage->yRes()));
		double maxRadiusX = m_canvas->viewConverter()->viewToDocumentX(m_maxRadius);
		double maxRadiusY = m_canvas->viewConverter()->viewToDocumentY(m_maxRadius);
		m_canvas->updateCanvas(rc.adjusted(-maxRadiusX, -maxRadiusY, maxRadiusX, maxRadiusY));
	}
}

void KisToolTransform::updateOutlineChanged()
{
	KisImageSP kisimage = image();
	double maxRadiusX = m_canvas->viewConverter()->viewToDocumentX(m_maxRadius);
	double maxRadiusY = m_canvas->viewConverter()->viewToDocumentY(m_maxRadius);

	//get the smallest rectangle containing the previous frame (we need to use the 4 points because the rectangle
	//described by m_topLeft, .., m_bottomLeft can be rotated
	QRectF oldRectF = boundRect(m_topLeftProj, m_topRightProj, m_bottomRightProj, m_bottomLeftProj);
	//we convert it to the right scale
	QRect oldRect = QRect( QPoint(oldRectF.left() / kisimage->xRes(), oldRectF.top() / kisimage->yRes()), QPoint(oldRectF.right() / kisimage->xRes(), oldRectF.bottom() / kisimage->yRes()) );

	recalcOutline(); //computes new m_topLeft, .., m_bottomLeft points
	QRectF newRectF = boundRect(m_topLeftProj, m_topRightProj, m_bottomRightProj, m_bottomLeftProj);
	QRect newRect = QRect( QPoint(newRectF.left() / kisimage->xRes(), newRectF.top() / kisimage->yRes()), QPoint(newRectF.right() / kisimage->xRes(), newRectF.bottom() / kisimage->yRes()) );

	//the rectangle to update is the union of the old rectangle et the new one
	newRect = oldRect.united(newRect);

	//we need to add adjust the rectangle because of the handles
	newRect.adjust(- maxRadiusX, - maxRadiusY, maxRadiusX, maxRadiusY);
	m_canvas->updateCanvas(newRect);
}

void KisToolTransform::refreshSpinBoxes()
{
	if (m_optWidget == 0)
		return;

	if (m_optWidget->scaleXBox)
		m_optWidget->scaleXBox->setValue(m_currentArgs.scaleX() * 100.);
	if (m_optWidget->scaleYBox)
		m_optWidget->scaleYBox->setValue(m_currentArgs.scaleY() * 100.);
	if (m_optWidget->shearXBox)
		m_optWidget->shearXBox->setValue(m_currentArgs.shearX());
	if (m_optWidget->shearYBox)
		m_optWidget->shearYBox->setValue(m_currentArgs.shearY());
	if (m_optWidget->translateXBox)
		m_optWidget->translateXBox->setValue(m_currentArgs.translate().x());
	if (m_optWidget->translateYBox)
		m_optWidget->translateYBox->setValue(m_currentArgs.translate().y());
	if (m_optWidget->aXBox)
		m_optWidget->aXBox->setValue(radianToDegree(m_currentArgs.aX()));
	if (m_optWidget->aYBox)
		m_optWidget->aYBox->setValue(radianToDegree(m_currentArgs.aY()));
	if (m_optWidget->aZBox)
		m_optWidget->aZBox->setValue(radianToDegree(m_currentArgs.aZ()));
}

void KisToolTransform::setButtonBoxDisabled(bool disabled)
{
	if (m_optWidget && m_optWidget->buttonBox) {
		QAbstractButton *applyButton = m_optWidget->buttonBox->button(QDialogButtonBox::Apply);
		QAbstractButton *resetButton = m_optWidget->buttonBox->button(QDialogButtonBox::Reset);

		if (applyButton)
			applyButton->setDisabled(disabled);
		if (resetButton)
			resetButton->setDisabled(disabled);
	}
}

void KisToolTransform::deactivate()
{
    KisImageWSP kisimage = image();

	updateCurrentOutline();

	if (!kisimage)
		return;

    if (kisimage->undoAdapter())
        kisimage->undoAdapter()->removeCommandHistoryListener(this);
}

void KisToolTransform::activate(ToolActivation toolActivation, const QSet<KoShape*> &)
{
    Q_UNUSED(toolActivation);

    if (currentNode() && currentNode()->paintDevice()) {
        image()->undoAdapter()->setCommandHistoryListener(this);

        const ApplyTransformCmdData * presentCmd1 = 0;
        const TransformCmd* presentCmd2 = 0;

        if (image()->undoAdapter()->presentCommand()) {
            presentCmd1 = dynamic_cast<const ApplyTransformCmdData*>(image()->undoAdapter()->presentCommand());
            presentCmd2 = dynamic_cast<const TransformCmd*>(image()->undoAdapter()->presentCommand());
		}

        if (presentCmd1 == 0 && presentCmd2 == 0) {
            initHandles();
			refreshSpinBoxes();
        } else {
            // One of our commands is on top
            // We should ask for tool args and orig selection
			if (presentCmd1 != 0) {
				presentCmd1->transformArgs(m_currentArgs);
				m_origSelection = presentCmd1->origSelection(m_originalTopLeft, m_originalBottomRight);
				presentCmd1->origPreviews(m_origImg, m_origSelectionImg);

				setButtonBoxDisabled(true);
			} else {
				presentCmd2->transformArgs(m_currentArgs);
				m_origSelection = presentCmd2->origSelection(m_originalTopLeft, m_originalBottomRight);
				presentCmd2->origPreviews(m_origImg, m_origSelectionImg);

				setButtonBoxDisabled(false);
			}

			m_originalCenter = (m_originalTopLeft + m_originalBottomRight) / 2;
			m_previousTopLeft = m_originalTopLeft;
			m_previousBottomRight = m_originalBottomRight;
			m_scaleX_wOutModifier = m_currentArgs.scaleX();
			m_scaleY_wOutModifier = m_currentArgs.scaleY();
			m_refSize = QSizeF(0, 0); //will force the recalc of image in recalcOutline

			updateOutlineChanged();
			refreshSpinBoxes();
        }
    }
    currentNode() =
        m_canvas->resourceManager()->resource(KisCanvasResourceProvider::CurrentKritaNode).value<KisNodeSP>();
}

void KisToolTransform::initTransform()
{
	m_previousTopLeft = m_originalTopLeft;
	m_previousBottomRight = m_originalBottomRight;
	m_currentArgs = ToolTransformArgs(m_originalCenter, QPointF(0, 0), 0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0);
    m_scaleX_wOutModifier = m_currentArgs.scaleX();
    m_scaleY_wOutModifier = m_currentArgs.scaleY();
	m_refSize = QSizeF(0, 0);
}

void KisToolTransform::initHandles()
{
    int x, y, w, h;

    KisPaintDeviceSP dev = currentNode()->paintDevice();

    // Create a lazy copy of the current state
    m_origDevice = new KisPaintDevice(*dev.data());
    Q_ASSERT(m_origDevice);

    KisSelectionSP selection = currentSelection();
    if (selection) {
        QRect r = selection->selectedExactRect();
        m_origSelection = new KisSelection();
        KisPixelSelectionSP origPixelSelection = new KisPixelSelection(*selection->getOrCreatePixelSelection().data());
        m_origSelection->setPixelSelection(origPixelSelection);
        r.getRect(&x, &y, &w, &h);

		//if (selection->hasShapeSelection())
		//{
		//	//save the state of the shapes
        //    KisShapeSelection* shapeSelection = static_cast<KisShapeSelection*>(selection->shapeSelection());
		//	m_origSelection->setShapeSelection(shapeSelection->clone(selection.data()));
		//}
    } else {
		//we take all of the paintDevice
        dev->exactBounds(x, y, w, h);
        m_origSelection = 0;
    }
    m_originalTopLeft = QPoint(x, y);
    m_originalBottomRight = QPoint(x + w - 1, y + h - 1);
    m_originalCenter = QPointF(m_originalTopLeft + m_originalBottomRight) / 2.0;
	m_originalHeight2 = m_originalCenter.y() - m_originalTopLeft.y();
	m_originalWidth2 = m_originalCenter.x() - m_originalTopLeft.x();

	initTransform();

	const KisImage *kisimage = image();
	m_transform = QTransform();
	m_origImg = new QImage(m_origDevice->convertToQImage(0, x, y, w, h));
	if (selection)
		m_origSelectionImg = new QImage(selection->convertToQImage(0, x, y, w, h));
	else {
		m_origSelectionImg = new QImage(w, h, QImage::Format_ARGB32);
		m_origSelectionImg->fill(0xFFFFFFFF);
	}
	m_origSelectionImg->invertPixels();
	QImage alphaChannel = m_origSelectionImg->createMaskFromColor(qRgb(0, 0, 0));
	m_origImg->setAlphaChannel(alphaChannel);
	m_currImg = QImage(*m_origImg); //create a shallow copy
	m_origSelectionImg->setAlphaChannel(alphaChannel);
	if (m_canvas && m_canvas->viewConverter() && kisimage)
		m_refSize = m_canvas->viewConverter()->documentToView(QSizeF(1 / kisimage->xRes(), 1 / kisimage->yRes()));
	else
		m_refSize = QSizeF(1, 1);
	m_scaledOrigSelectionImg = m_origSelectionImg->scaled(m_refSize.width() * m_origSelectionImg->width(), m_refSize.height() * m_origSelectionImg->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	updateOutlineChanged();
}

void KisToolTransform::mousePressEvent(KoPointerEvent *e)
{
    KisImageWSP kisimage = image();

    if (!currentNode())
        return;


    if (kisimage && currentNode()->paintDevice() && e->button() == Qt::LeftButton) {
        QPointF mousePos = QPointF(e->point.x() * kisimage->xRes(), e->point.y() * kisimage->yRes());
		if (m_function == ROTATE) {
			QPointF clickoffset = mousePos - m_rotationCenter.toPointF();
            m_clickangle = atan2(-clickoffset.y(), clickoffset.x());
		}

		m_clickPoint = mousePos;
        m_selecting = true;
        m_actuallyMoveWhileSelected = false;
		m_prevMousePos = mousePos;
    }

	m_clickRotationCenter = m_rotationCenter;
	storeArgs(m_clickArgs);

	recalcOutline();
}

int KisToolTransform::det(const QPointF & v, const QPointF & w)
{
    return int(v.x()*w.y() - v.y()*w.x());
}

double KisToolTransform::distsq(const QPointF & v, const QPointF & w)
{
    QPointF v2 = v - w;
    return v2.x()*v2.x() + v2.y()*v2.y();
} 

int KisToolTransform::octant(double x, double y)
{
	double angle = atan2(- y, x) + M_PI / 8;
	//M_PI / 8 to get the correct octant

	//we want an angle in [0; 2 * Pi[
	angle = fmod(angle, 2. * M_PI);
	if (angle < 0)
		angle += 2 * M_PI;

	int octant = (int)(angle * 4. / M_PI);

	return octant;
}

void KisToolTransform::setFunctionalCursor()
{
	QVector3D dir_vect;
	int rotOctant;

    switch (m_function) {
    case MOVE:
        useCursor(KisCursor::moveCursor());
        break;
    case ROTATE:
        useCursor(KisCursor::rotateCursor());
        break;
	case RIGHTSCALE:
	case TOPRIGHTSCALE:
	case TOPSCALE:
	case TOPLEFTSCALE:
	case LEFTSCALE:
	case BOTTOMLEFTSCALE:
	case BOTTOMSCALE:
	case BOTTOMRIGHTSCALE:
		dir_vect = QVector3D(m_handleDir[m_function - RIGHTSCALE]);
		if (m_currentArgs.scaleX() < 0)
			dir_vect.setX(- dir_vect.x());
		if (m_currentArgs.scaleY() < 0)
			dir_vect.setY(- dir_vect.y());
		dir_vect = rotZ(dir_vect.x(), dir_vect.y(), dir_vect.z());
		dir_vect = rotY(dir_vect.x(), dir_vect.y(), dir_vect.z());
		dir_vect = rotX(dir_vect.x(), dir_vect.y(), dir_vect.z());
		dir_vect = QVector3D(perspective(dir_vect.x(), dir_vect.y(), dir_vect.z()));

		rotOctant = octant(dir_vect.x(), dir_vect.y());
        useCursor(m_sizeCursors[rotOctant]);
        break;
	case MOVECENTER:
		useCursor(KisCursor::handCursor());
		break;
	case BOTTOMSHEAR:
	case RIGHTSHEAR:
	case TOPSHEAR:
	case LEFTSHEAR:
		dir_vect = QVector3D(m_handleDir[(m_function - BOTTOMSHEAR) * 2]);
		if (m_currentArgs.scaleX() < 0)
			dir_vect.setX(- dir_vect.x());
		if (m_currentArgs.scaleY() < 0)
			dir_vect.setY(- dir_vect.y());
		dir_vect = rotZ(dir_vect.x(), dir_vect.y(), dir_vect.z());
		dir_vect = rotY(dir_vect.x(), dir_vect.y(), dir_vect.z());
		dir_vect = rotX(dir_vect.x(), dir_vect.y(), dir_vect.z());
		dir_vect = QVector3D(perspective(dir_vect.x(), dir_vect.y(), dir_vect.z()));

		rotOctant = octant(dir_vect.x(), dir_vect.y());
        useCursor(m_sizeCursors[rotOctant]);
    }
}

void KisToolTransform::mouseMoveEvent(KoPointerEvent *e)
{
    KisCanvas2 *canvas = dynamic_cast<KisCanvas2 *>(m_canvas);
    if (!canvas)
        return;

    KisImageWSP kisimage = image();
    QPointF mousePos = QPointF(e->point.x() * kisimage->xRes(), e->point.y() * kisimage->yRes());
	double dx, dy;

    if (m_selecting) {
        m_actuallyMoveWhileSelected = true;

		QPointF move_vect = mousePos - m_prevMousePos;

		double previousHeight2 = 0, previousWidth2 = 0;
		int signY = 1, signX = 1;
		QVector3D t(0,0,0), t2(0, 0, 0);

		switch (m_function) {
		case MOVE:
			t = QVector3D(mousePos - m_clickPoint);
			if (e->modifiers() & Qt::ShiftModifier) {
				if (e->modifiers() & Qt::ControlModifier) {
					t = invTransformVector(t.x(), t.y(), t.z()); //go to local/object space
					if (fabs(t.x()) >= fabs(t.y()))
						t.setY(0);
					else
						t.setX(0);
					t = transformVector(t.x(), t.y(), t.z()); //go back to global space
				} else {
					if (fabs(t.x()) >= fabs(t.y()))
						t.setY(0);
					else
						t.setX(0);
				}
			}

			m_currentArgs.setTranslate(m_clickArgs.translate() + t.toPointF());
			m_optWidget->translateXBox->setValue(m_currentArgs.translate().x());
			m_optWidget->translateYBox->setValue(m_currentArgs.translate().y());
			break;
		case ROTATE:
			{
				double theta = m_clickangle - atan2(-mousePos.y() + m_clickRotationCenter.y(), mousePos.x() - m_clickRotationCenter.x());

				if (e->modifiers() & Qt::ShiftModifier) {
					int quotient = theta * 12 / M_PI;
					theta = quotient * M_PI / 12;
				}
				std::complex<qreal> exp_theta = exp(std::complex<qreal>(0, theta));
				std::complex<qreal> rotationCenter(m_clickRotationCenter.x(), m_clickRotationCenter.y());
				std::complex<qreal> origin(m_clickArgs.translate().x(), m_clickArgs.translate().y());
				std::complex<qreal> translate((exp_theta - std::complex<qreal>(1, 0)) * (origin - rotationCenter));
				
				m_currentArgs.setTranslate(m_clickArgs.translate() + QPointF(translate.real(), translate.imag()));
				m_currentArgs.setAZ(m_clickArgs.aZ() + theta);

				m_optWidget->translateXBox->setValue(m_currentArgs.translate().x());
				m_optWidget->translateYBox->setValue(m_currentArgs.translate().y());
				m_optWidget->aZBox->setValue(radianToDegree(m_currentArgs.aZ()));
			}
			break;
		case TOPSCALE:
			signY = -1;
		case BOTTOMSCALE:
			// transform move_vect coords, so it seems like it isn't rotated and centered at m_translate
			t = QVector3D(move_vect);
			t = invrotX(t.x(), t.y(), t.z());
			t = invrotY(t.x(), t.y(), t.z());
			t = invrotZ(t.x(), t.y(), t.z());
			t = invshear(t.x(), t.y(), t.z());
			move_vect = t.toPointF();

			move_vect /= 2;
			m_scaleY_wOutModifier = (m_scaleY_wOutModifier * m_originalHeight2 + signY * move_vect.y()) / m_originalHeight2;
			previousHeight2 = m_currentArgs.scaleY() * m_originalHeight2;

			//applies the shift modifier
			if (e->modifiers() & Qt::ShiftModifier) {
				double a_scaleY = fabs(m_scaleY_wOutModifier);

				m_currentArgs.setScaleX((m_scaleX_wOutModifier > 0) ? a_scaleY : -a_scaleY);
				m_currentArgs.setScaleY(m_scaleY_wOutModifier);
			} else
				m_currentArgs.setScaleY(m_scaleY_wOutModifier);

			t.setX(0);
			t.setY(signY * (m_originalHeight2 * m_currentArgs.scaleY() - previousHeight2));
			t.setZ(0);
			t = shear(t.x(), t.y(), t.z());
			t = rotZ(t.x(), t.y(), t.x());
			t = rotY(t.x(), t.y(), t.x());
			t = rotX(t.x(), t.y(), t.x());

			m_currentArgs.setTranslate(m_currentArgs.translate() + perspective(t.x(), t.y(), t.z()));

			m_optWidget->translateXBox->setValue(m_currentArgs.translate().x());
			m_optWidget->translateYBox->setValue(m_currentArgs.translate().y());
			m_optWidget->scaleXBox->setValue(m_currentArgs.scaleX() * 100.);
			m_optWidget->scaleYBox->setValue(m_currentArgs.scaleY() * 100.);
			break;
		case LEFTSCALE:
			signX = -1;
		case RIGHTSCALE:
			t = QVector3D(move_vect);
			t = invrotX(t.x(), t.y(), t.z());
			t = invrotY(t.x(), t.y(), t.z());
			t = invrotZ(t.x(), t.y(), t.z());
			t = invshear(t.x(), t.y(), t.z());
			move_vect = t.toPointF();

			move_vect /= 2;
			m_scaleX_wOutModifier = (m_scaleX_wOutModifier * m_originalWidth2 + signX * move_vect.x()) / m_originalWidth2;
			previousWidth2 = m_currentArgs.scaleX() * m_originalWidth2;

			//applies the shift modifier
			if (e->modifiers() & Qt::ShiftModifier) {
				double a_scaleX = fabs(m_scaleX_wOutModifier);

				m_currentArgs.setScaleY((m_scaleY_wOutModifier > 0) ? a_scaleX : -a_scaleX);
				m_currentArgs.setScaleX(m_scaleX_wOutModifier);
			} else
				m_currentArgs.setScaleX(m_scaleX_wOutModifier);

			t.setX(signX * (m_originalWidth2 * m_currentArgs.scaleX() - previousWidth2));
			t.setY(0);
			t.setZ(0);
			t = shear(t.x(), t.y(), t.z());
			t = rotZ(t.x(), t.y(), t.z());
			t = rotY(t.x(), t.y(), t.z());
			t = rotX(t.x(), t.y(), t.z());

			m_currentArgs.setTranslate(m_currentArgs.translate() + perspective(t.x(), t.y(), t.z()));

			m_optWidget->translateXBox->setValue(m_currentArgs.translate().x());
			m_optWidget->translateYBox->setValue(m_currentArgs.translate().y());
			m_optWidget->scaleXBox->setValue(m_currentArgs.scaleX() * 100.);
			m_optWidget->scaleYBox->setValue(m_currentArgs.scaleY() * 100.);
			break;
		case TOPRIGHTSCALE:
		case BOTTOMRIGHTSCALE:
		case TOPLEFTSCALE:
		case BOTTOMLEFTSCALE: 
			switch(m_function) {
			case TOPRIGHTSCALE:
				signY = -1;
			case BOTTOMRIGHTSCALE:
				break;
			case TOPLEFTSCALE:
				signY = -1;
			case BOTTOMLEFTSCALE:
				signX = -1;
				break;
			default:
				break;
			}

			t = QVector3D(move_vect);
			t = invrotX(t.x(), t.y(), t.z());
			t = invrotY(t.x(), t.y(), t.z());
			t = invrotZ(t.x(), t.y(), t.z());
			t = invshear(t.x(), t.y(), t.z());
			move_vect = t.toPointF();
			move_vect /= 2;
			m_scaleX_wOutModifier = (m_scaleX_wOutModifier * m_originalWidth2 + signX * move_vect.x()) / m_originalWidth2;
			m_scaleY_wOutModifier = (m_scaleY_wOutModifier * m_originalHeight2 + signY * move_vect.y()) / m_originalHeight2;
			previousWidth2 = m_currentArgs.scaleX() * m_originalWidth2;
			previousHeight2 = m_currentArgs.scaleY() * m_originalHeight2;

			//applies the shift modifier
			if (e->modifiers() & Qt::ShiftModifier) {
				double a_scaleX = fabs(m_scaleX_wOutModifier);
				double a_scaleY = fabs(m_scaleY_wOutModifier);

				if (a_scaleX > a_scaleY) {
					m_currentArgs.setScaleY((m_scaleY_wOutModifier > 0) ? a_scaleX : -a_scaleX);
					m_currentArgs.setScaleX(m_scaleX_wOutModifier);
				} else {
					m_currentArgs.setScaleX((m_scaleX_wOutModifier > 0) ? a_scaleY : -a_scaleY);
					m_currentArgs.setScaleY(m_scaleY_wOutModifier);
				}
			} else {
				m_currentArgs.setScaleX(m_scaleX_wOutModifier);
				m_currentArgs.setScaleY(m_scaleY_wOutModifier);
			}

			t.setX(signX * (m_originalWidth2 * m_currentArgs.scaleX() - previousWidth2));
			t.setY(signY * (m_originalHeight2 * m_currentArgs.scaleY() - previousHeight2));
			t = shear(t.x(), t.y(), t.z());
			t = rotX(t.x(), t.y(), t.z());
			t = rotY(t.x(), t.y(), t.z());
			t = rotZ(t.x(), t.y(), t.z());

			m_currentArgs.setTranslate(m_currentArgs.translate() + perspective(t.x(), t.y(), t.z()));

			m_optWidget->translateXBox->setValue(m_currentArgs.translate().x());
			m_optWidget->translateYBox->setValue(m_currentArgs.translate().y());
			m_optWidget->scaleXBox->setValue(m_currentArgs.scaleX() * 100.);
			m_optWidget->scaleYBox->setValue(m_currentArgs.scaleY() * 100.);
	 		break;
		case MOVECENTER:
			t = QVector3D(mousePos - m_currentArgs.translate());

			if (e->modifiers() & Qt::ShiftModifier) {
				if (e->modifiers() & Qt::ControlModifier) {
					//we apply the constraint before going to local space
					if (fabs(t.x()) >= fabs(t.y()))
						t.setY(0);
					else
						t.setX(0);
					t = invTransformVector(t.x(), t.y(), t.z()); //go to local space
				} else {
					t = invTransformVector(t.x(), t.y(), t.z()); //go to local space
					if (fabs(t.x()) >= fabs(t.y()))
						t.setY(0);
					else
						t.setX(0);
					//stay in local space because the center offset is taken in local space
				}
			} else
				t = invTransformVector(t.x(), t.y(), t.z());

			//now we need to clip t in the rectangle of the tool
			if (t.y() != 0) {
				double slope = t.y() / t.x();

				if (t.x() < - m_originalWidth2) {
					t.setY(- m_originalWidth2 * slope);
					t.setX(- m_originalWidth2);
				} else if (t.x() > m_originalWidth2) {
					t.setY(m_originalWidth2 * slope);
					t.setX(m_originalWidth2);
				}

				if (t.y() < - m_originalHeight2) {
					if (t.x() != 0)
						t.setX(- m_originalHeight2 / slope);
					t.setY(- m_originalHeight2);
				} else if (t.y() > m_originalHeight2) {
					if (t.x() != 0)
						t.setX(m_originalHeight2 / slope);
					t.setY(m_originalHeight2);
				}
			} else {
				if (t.x() < - m_originalWidth2)
					t.setX(- m_originalWidth2);
				else if (t.x() > m_originalWidth2)
					t.setX(m_originalWidth2);
			}

			m_currentArgs.setRotationCenterOffset(t.toPointF());

			if (m_rotCenterButtons->checkedId() >= 0 && m_rotCenterButtons->checkedId() < 9)
				m_rotCenterButtons->button(9)->setChecked(true); //uncheck the current checked button
			break;
		case TOPSHEAR:
			signX = -1;
		case BOTTOMSHEAR:
			signX *= (m_currentArgs.scaleY() < 0) ? -1 : 1;
			t = QVector3D(mousePos - m_clickPoint);
			t = invrotX(t.x(), t.y(), t.z());
			t = invrotY(t.x(), t.y(), t.z());
			t = invrotZ(t.x(), t.y(), t.z());
			//we do not use invShear because we want to use the clickArgs shear factors
			t.setY(t.y() - t.x() * m_clickArgs.shearY());
			t.setX(t.x() - t.y() * m_clickArgs.shearX());

			dx = signX *  m_clickArgs.shearX() * m_originalHeight2; //get the dx pixels corresponding to the current shearX factor
			dx += t.x(); //add the horizontal movement
			m_currentArgs.setShearX(signX * dx / m_originalHeight2); // calculate the new shearX factor

			m_optWidget->shearXBox->setValue(m_currentArgs.shearX());
			break;
		case LEFTSHEAR:
			signY = -1;
		case RIGHTSHEAR:
			signY *= (m_currentArgs.scaleX() < 0) ? -1 : 1;
			t = QVector3D(mousePos - m_clickPoint);
			t = invrotX(t.x(), t.y(), t.z());
			t = invrotY(t.x(), t.y(), t.z());
			t = invrotZ(t.x(), t.y(), t.z());
			t.setY(t.y() - t.x() * m_clickArgs.shearY());
			t.setX(t.x() - t.y() * m_clickArgs.shearX());

			dy = signY *  m_clickArgs.shearY() * m_originalWidth2; //get the dx pixels corresponding to the current shearX factor
			dy += t.y(); //add the horizontal movement
			m_currentArgs.setShearY(signY * dy / m_originalWidth2); // calculate the new shearX factor

			m_optWidget->shearYBox->setValue(m_currentArgs.shearY());
			break;
		}

		updateOutlineChanged();
    } else {
		recalcOutline();
		
		QPointF *topLeft, *topRight, *bottomLeft, *bottomRight;
		QPointF *tmp;

		//depending on the scale factor, we need to exchange left<->right and right<->left
		if (m_currentArgs.scaleX() > 0) {
			topLeft = &m_topLeftProj;
			bottomLeft = &m_bottomLeftProj;
			topRight = &m_topRightProj;
			bottomRight = &m_bottomRightProj;
		} else {
			topLeft = &m_topRightProj;
			bottomLeft = &m_bottomRightProj;
			topRight = &m_topLeftProj;
			bottomRight = &m_bottomLeftProj;
		}

		if (m_currentArgs.scaleY() < 0) {
			tmp = topLeft;
			topLeft = bottomLeft;
			bottomLeft = tmp;
			tmp = topRight;
			topRight = bottomRight;
			bottomRight = tmp;
		}

        if (det(mousePos - *topLeft, *topRight - *topLeft) > 0)
            m_function = ROTATE;
        else if (det(mousePos - *topRight, *bottomRight - *topRight) > 0)
            m_function = ROTATE;
        else if (det(mousePos - *bottomRight, *bottomLeft - *bottomRight) > 0)
            m_function = ROTATE;
        else if (det(mousePos - *bottomLeft, *topLeft - *bottomLeft) > 0)
            m_function = ROTATE;
        else
            m_function = MOVE;

        double handleRadiusX = m_canvas->viewConverter()->viewToDocumentX(m_handleRadius);
        double handleRadiusY = m_canvas->viewConverter()->viewToDocumentY(m_handleRadius);
        double handleRadius = (handleRadiusX > handleRadiusY) ? handleRadiusX : handleRadiusY;
        double handleRadiusSq = handleRadius * handleRadius; // square it so it fits with distsq

		double rotationCenterRadiusX = m_canvas->viewConverter()->viewToDocumentX(m_rotationCenterRadius);
		double rotationCenterRadiusY = m_canvas->viewConverter()->viewToDocumentY(m_rotationCenterRadius);
        double rotationCenterRadius = (rotationCenterRadiusX > rotationCenterRadiusY) ? rotationCenterRadiusX : rotationCenterRadiusY;
		rotationCenterRadius *= rotationCenterRadius;

        if (distsq(mousePos, m_middleTopProj) <= handleRadiusSq)
            m_function = TOPSCALE;
        if (distsq(mousePos, m_topRightProj) <= handleRadiusSq)
            m_function = TOPRIGHTSCALE;
        if (distsq(mousePos, m_middleRightProj) <= handleRadiusSq)
            m_function = RIGHTSCALE;
        if (distsq(mousePos, m_bottomRightProj) <= handleRadiusSq)
            m_function = BOTTOMRIGHTSCALE;
        if (distsq(mousePos, m_middleBottomProj) <= handleRadiusSq)
            m_function = BOTTOMSCALE;
        if (distsq(mousePos, m_bottomLeftProj) <= handleRadiusSq)
            m_function = BOTTOMLEFTSCALE;
        if (distsq(mousePos, m_middleLeftProj) <= handleRadiusSq)
            m_function = LEFTSCALE;
        if (distsq(mousePos, m_topLeftProj) <= handleRadiusSq)
            m_function = TOPLEFTSCALE;
		if (distsq(mousePos, m_rotationCenterProj) <= rotationCenterRadius)
			m_function = MOVECENTER;

		if (m_function == ROTATE || m_function == MOVE) {
			//We check for shearing only if we aren't near a handle (for scale) or the rotation center
			QPointF t = invTransformVector(QVector3D(mousePos - m_currentArgs.translate())).toPointF();
			t += m_originalCenter;
	 
			if (t.x() >= m_originalTopLeft.x() && t.x() <= m_originalBottomRight.x()) {
				if (fabs(t.y() - m_originalTopLeft.y()) <= 12)
					m_function = TOPSHEAR;
				else if (fabs(t.y() - m_originalBottomRight.y()) <= 12)
					m_function = BOTTOMSHEAR;
			} else if (t.y() >= m_originalTopLeft.y() && t.y() <= m_originalBottomRight.y()) {
				if (fabs(t.x() - m_originalTopLeft.x()) <= 12)
					m_function = LEFTSHEAR;
				else if (fabs(t.x() - m_originalBottomRight.x()) <= 12)
					m_function = RIGHTSHEAR;
			}
		}

        setFunctionalCursor();
    }

	m_prevMousePos = mousePos;
}

void KisToolTransform::mouseReleaseEvent(KoPointerEvent */*e*/)
{
    m_selecting = false;

    if (m_actuallyMoveWhileSelected) {
		transform();

		m_scaleX_wOutModifier = m_currentArgs.scaleX();
		m_scaleY_wOutModifier = m_currentArgs.scaleY();
    }
}

void KisToolTransform::recalcOutline()
{
	QVector3D t;
	QVector3D translate3D(m_currentArgs.translate());
	QVector3D d;

    m_sinaX = sin(m_currentArgs.aX());
    m_cosaX = cos(m_currentArgs.aX());
    m_sinaY = sin(m_currentArgs.aY());
    m_cosaY = cos(m_currentArgs.aY());
    m_sinaZ = sin(m_currentArgs.aZ());
    m_cosaZ = cos(m_currentArgs.aZ());

	t = transformVector(QVector3D(m_originalTopLeft - m_originalCenter));
	m_topLeft = t + translate3D;
	m_topLeftProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector(m_originalBottomRight.x() - m_originalCenter.x(), m_originalTopLeft.y() - m_originalCenter.y(), 0);
	m_topRight = t + translate3D;
	m_topRightProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector(m_originalTopLeft.x() - m_originalCenter.x(), m_originalBottomRight.y() - m_originalCenter.y(), 0);
	m_bottomLeft = t + translate3D;
	m_bottomLeftProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector(QVector3D(m_originalBottomRight - m_originalCenter));
	m_bottomRight = t + translate3D;
	m_bottomRightProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector(m_originalTopLeft.x() - m_originalCenter.x(), (m_originalTopLeft.y() + m_originalBottomRight.y()) / 2.0 - m_originalCenter.y(), 0);
	m_middleLeft = t + translate3D;
	m_middleLeftProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector(m_originalBottomRight.x() - m_originalCenter.x(), (m_originalTopLeft.y() + m_originalBottomRight.y()) / 2.0 - m_originalCenter.y(), 0);
	m_middleRight = t + translate3D;
	m_middleRightProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector((m_originalTopLeft.x() + m_originalBottomRight.x()) / 2.0 - m_originalCenter.x(), m_originalTopLeft.y() - m_originalCenter.y(), 0);
	m_middleTop = t + translate3D;
	m_middleTopProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector((m_originalTopLeft.x() + m_originalBottomRight.x()) / 2.0 - m_originalCenter.x(), m_originalBottomRight.y() - m_originalCenter.y(), 0);
	m_middleBottom = t + translate3D;
	m_middleBottomProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	t = transformVector(QVector3D(m_currentArgs.rotationCenterOffset()));
	m_rotationCenter = t + translate3D;
	m_rotationCenterProj = perspective(t.x(), t.y(), t.z()) + m_currentArgs.translate();

	KisImageSP kisimage = image();
	QSizeF s(1 / kisimage->xRes(), 1 / kisimage->yRes());
	s = m_canvas->viewConverter()->documentToView(s);
	m_transform = QTransform();
	m_transform.rotateRadians(m_currentArgs.aZ(), Qt::ZAxis);
	m_transform.shear(0, m_currentArgs.shearY());
	m_transform.shear(m_currentArgs.shearX(), 0);
	m_transform.scale(m_currentArgs.scaleX() * s.width(), m_currentArgs.scaleY() * s.height());
	m_currImg = m_origImg->transformed(m_transform);
}

void KisToolTransform::paint(QPainter& gc, const KoViewConverter &converter)
{
    QPen old = gc.pen();
    QPen pen(Qt::SolidLine);
    pen.setWidth(0);

    KisImageWSP kisimage = image();
	if (kisimage == NULL)
		return;
	QSizeF newRefSize = converter.documentToView(QSizeF(1 / kisimage->xRes(), 1 / kisimage->yRes()));
    QPointF topleft = converter.documentToView(QPointF(m_topLeftProj.x() / kisimage->xRes(), m_topLeftProj.y() / kisimage->yRes()));
    QPointF topright = converter.documentToView(QPointF(m_topRightProj.x() / kisimage->xRes(), m_topRightProj.y() / kisimage->yRes()));
    QPointF bottomleft = converter.documentToView(QPointF(m_bottomLeftProj.x() / kisimage->xRes(), m_bottomLeftProj.y() / kisimage->yRes()));
    QPointF bottomright = converter.documentToView(QPointF(m_bottomRightProj.x() / kisimage->xRes(), m_bottomRightProj.y() / kisimage->yRes()));
    QPointF middleleft = converter.documentToView(QPointF(m_middleLeftProj.x() / kisimage->xRes(), m_middleLeftProj.y() / kisimage->yRes()));
    QPointF middleright = converter.documentToView(QPointF(m_middleRightProj.x() / kisimage->xRes(), m_middleRightProj.y() / kisimage->yRes()));
    QPointF middletop = converter.documentToView(QPointF(m_middleTopProj.x() / kisimage->xRes(), m_middleTopProj.y() / kisimage->yRes()));
    QPointF middlebottom = converter.documentToView(QPointF(m_middleBottomProj.x() / kisimage->xRes(), m_middleBottomProj.y() / kisimage->yRes()));
    QPointF origtopleft = converter.documentToView(QPointF(m_originalTopLeft.x() / kisimage->xRes(), m_originalTopLeft.y() / kisimage->yRes()));
    QPointF origbottomright = converter.documentToView(QPointF(m_originalBottomRight.x() / kisimage->xRes(), m_originalBottomRight.y() / kisimage->yRes()));

    QRectF handleRect(- m_handleRadius / 2., - m_handleRadius / 2., m_handleRadius, m_handleRadius);

    gc.setPen(pen);

	if (newRefSize != m_refSize) {
		//need to update m_scaledOrigSelectionImg and m_currentImg
		m_refSize = newRefSize;
		recalcOutline();
		m_scaledOrigSelectionImg = m_origSelectionImg->scaled(m_refSize.width() * m_origSelectionImg->width(), m_refSize.height() * m_origSelectionImg->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	gc.setOpacity(0.6);
	gc.drawImage(origtopleft, m_scaledOrigSelectionImg, QRectF(m_scaledOrigSelectionImg.rect()));

	QRectF bRect = boundRect(topleft, topright, bottomleft, bottomright);
	gc.drawImage(QPointF(bRect.left(), bRect.top()), m_currImg, QRectF(m_currImg.rect()));
	gc.setOpacity(1.0);
	
    gc.drawRect(handleRect.translated(topleft));
    gc.drawRect(handleRect.translated(middletop));
    gc.drawRect(handleRect.translated(topright));
    gc.drawRect(handleRect.translated(middleright));
    gc.drawRect(handleRect.translated(bottomright));
    gc.drawRect(handleRect.translated(middlebottom));
    gc.drawRect(handleRect.translated(bottomleft));
    gc.drawRect(handleRect.translated(middleleft));

    gc.drawLine(topleft, topright);
    gc.drawLine(topright, bottomright);
    gc.drawLine(bottomright, bottomleft);
    gc.drawLine(bottomleft, topleft);

	QPointF rotationCenter = converter.documentToView(QPointF(m_rotationCenterProj.x() / kisimage->xRes(), m_rotationCenterProj.y() / kisimage->yRes()));
	QRectF rotationCenterRect(- m_rotationCenterRadius / 2., - m_rotationCenterRadius / 2., m_rotationCenterRadius, m_rotationCenterRadius);
	gc.drawEllipse(rotationCenterRect.translated(rotationCenter));
	gc.drawLine(QPointF(rotationCenter.x() - m_rotationCenterRadius / 2. - 2, rotationCenter.y()), QPointF(rotationCenter.x() + m_rotationCenterRadius / 2. + 2, rotationCenter.y()));
	gc.drawLine(QPointF(rotationCenter.x(), rotationCenter.y() - m_rotationCenterRadius / 2. - 2), QPointF(rotationCenter.x(), rotationCenter.y() + m_rotationCenterRadius / 2. + 2));

    gc.setPen(old);
}

void KisToolTransform::transform()
{
    if (!image())
        return;

	setButtonBoxDisabled(false);
    TransformCmd *transaction = new TransformCmd(this, m_currentArgs, m_origSelection, m_originalTopLeft, m_originalBottomRight, m_origImg, m_origSelectionImg);

	if (image()->undoAdapter() != NULL)
		image()->undoAdapter()->addCommand(transaction);
}

void KisToolTransform::applyTransform()
{
    if (!image() || !currentNode()->paintDevice())
        return;

    KisCanvas2 *canvas = dynamic_cast<KisCanvas2 *>(m_canvas);
    if (!canvas)
        return;

	QVector3D tmpCenter(m_originalCenter.x(), m_originalCenter.y(), 0);
	tmpCenter = scale(tmpCenter.x(), tmpCenter.y(), tmpCenter.z());
	tmpCenter = rotZ(tmpCenter.x(), tmpCenter.y(), tmpCenter.z());
	QPointF t = m_currentArgs.translate() - tmpCenter.toPointF();
    KoProgressUpdater* updater = canvas->view()->createProgressUpdater();
    updater->start(100, i18n("Transform"));
    KoUpdaterPtr progress = updater->startSubtask();

	//todo/undo commented for now
    // This mementoes the current state of the active device.
	m_origImg->save(QString("test10.png"));
    ApplyTransformCmd transaction(this, currentNode(), m_currentArgs, m_origSelection, m_originalTopLeft, m_originalBottomRight, m_origImg, m_origSelectionImg);


    //Copy the original state back.
    QRect rc = m_origDevice->extent();
    rc = rc.normalized();
    currentNode()->paintDevice()->clear();
    KisPainter gc(currentNode()->paintDevice());
    gc.setCompositeOp(COMPOSITE_COPY);
    gc.bitBlt(rc.topLeft(), m_origDevice, rc);
    gc.end();

    // Also restore the original pixel selection (the shape selection will also be restored : see below)
    if (m_origSelection && !m_origSelection->isDeselected()) {
        if (currentSelection()) {
			//copy the pixel selection
			QRect rc = m_origSelection->selectedRect();
			rc = rc.normalized();
            currentSelection()->getOrCreatePixelSelection()->clear();
            KisPainter sgc(KisPaintDeviceSP(currentSelection()->getOrCreatePixelSelection()));
            sgc.setCompositeOp(COMPOSITE_COPY);
            sgc.bitBlt(rc.topLeft(), m_origSelection->getOrCreatePixelSelection(), rc);
            sgc.end();

        	//if (m_origSelection->hasShapeSelection()) {
			//	KisShapeSelection* origShapeSelection = static_cast<KisShapeSelection*>(m_origSelection->shapeSelection());
			//	if (currentSelection()->hasShapeSelection()) {
			//		KisShapeSelection* currentShapeSelection = static_cast<KisShapeSelection*>(currentSelection()->shapeSelection());
			//		m_previousShapeSelection = static_cast<KisShapeSelection*>(currentShapeSelection->clone(currentSelection().data()));
			//	}
			//	else
			//		m_previousShapeSelection = static_cast<KisShapeSelection*>(origShapeSelection->clone(m_origSelection.data()));

			//	currentSelection()->setShapeSelection(origShapeSelection->clone(m_origSelection.data()));
			//}
        }
    } else if (currentSelection())
        currentSelection()->clear();

    // Perform the transform. Since we copied the original state back, this doesn't degrade
    // after many tweaks. Since we started the transaction before the copy back, the memento
    // has the previous state.
    if (m_origSelection) {
		//we copy the pixels of the selection into a tmpDevice before clearing them
		//we apply the transformation to the tmpDevice
		//and then we blit it into the currentNode's device
        KisPaintDeviceSP tmpDevice = new KisPaintDevice(m_origDevice->colorSpace());
        QRect selectRect = currentSelection()->selectedExactRect();
        KisPainter gc(tmpDevice, currentSelection());
        gc.bitBlt(selectRect.topLeft(), m_origDevice, selectRect);
        gc.end();

        KisTransformWorker worker(tmpDevice, m_currentArgs.scaleX(), m_currentArgs.scaleY(), m_currentArgs.shearX(), m_currentArgs.shearY(), m_currentArgs.aZ(), int(t.x()), int(t.y()), progress, m_filter);
        worker.run();

        currentNode()->paintDevice()->clearSelection(currentSelection());

        QRect tmpRc = tmpDevice->extent();
        KisPainter painter(currentNode()->paintDevice());
        painter.bitBlt(tmpRc.topLeft(), tmpDevice, tmpRc);
        painter.end();

#ifdef KISPAINTDEVICE_MOVE_DEPRECATED
		//we do the same thing with the selection itself
		//we need to go through a temporary device because changing the offset of the
		//selection's paintDevice doesn't actually move the selection

		KisPixelSelectionSP pixelSelection = currentSelection()->getOrCreatePixelSelection();
		QRect pixelSelectRect = pixelSelection->selectedExactRect();

        KisPaintDeviceSP tmpDevice2 = new KisPaintDevice(pixelSelection->colorSpace());
        KisPainter gc2(tmpDevice2, currentSelection());
        gc2.bitBlt(pixelSelectRect.topLeft(), pixelSelection, pixelSelectRect);
        gc2.end();

        KisTransformWorker selectionWorker(tmpDevice2, m_currentArgs.scaleX(), m_currentArgs.scaleY(), m_currentArgs.shearX(), m_currentArgs.shearY(), m_currentArgs.aZ(), (int)(t.x()), (int)(t.y()), progress, m_filter);
        selectionWorker.run();

		pixelSelection->clear();

        QRect tmpRc2 = tmpDevice2->extent();
        KisPainter painter2(pixelSelection);
        painter2.bitBlt(tmpRc2.topLeft(), tmpDevice2, tmpRc2);
        painter2.end();
#else
        KisTransformWorker selectionWorker(currentSelection()->getOrCreatePixelSelection(), m_currentArgs.scaleX(), m_currentArgs.scaleY(), m_currentArgs.shearX(), m_currentArgs.shearY(), m_currentArgs.aZ(), (int)(t.x()), (int)(t.y()), progress, m_filter);
		selectionWorker.run();
#endif

		//Shape selection not transformed yet
        //if (m_origSelection->hasShapeSelection() && currentSelection()->hasShapeSelection() && m_previousShapeSelection) {
        //    KisShapeSelection* origShapeSelection = static_cast<KisShapeSelection*>(m_origSelection->shapeSelection());
        //    QList<KoShape *> origShapes = origShapeSelection->shapeManager()->shapes();
		//	KisShapeSelection* currentShapeSelection = static_cast<KisShapeSelection*>(currentSelection()->shapeSelection());
        //    QList<KoShape *> currentShapes = currentShapeSelection->shapeManager()->shapes();
        //    QList<KoShape *> previousShapes = m_previousShapeSelection->shapeManager()->shapes();
        //    QList<QMatrix> m_oldMatrixList;
        //    QList<QMatrix> m_newMatrixList;
		//	for (int i = 0; i < origShapes.size(); ++i) {
		//		KoShape *origShape = origShapes.at(i);
		//		QMatrix origMatrix = origShape->transformation();
		//		QMatrix previousMatrix = previousShapes.at(i)->transformation();

		//		QPointF center = origMatrix.map(QPointF(0.5 * origShape->size().width(), 0.5 * origShape->size().height()));
		//		QMatrix rotateMatrix;
		//		rotateMatrix.translate(center.x(), center.y());
		//		rotateMatrix.rotate(180. * m_a / M_PI);
		//		rotateMatrix.translate(-center.x(), -center.y());

        //        m_oldMatrixList << previousMatrix;
        //        m_newMatrixList << origMatrix * rotateMatrix; // we apply the rotation on the original matrix
        //    }
        //    KoShapeTransformCommand* cmd = new KoShapeTransformCommand(currentShapes, m_oldMatrixList, m_newMatrixList, transaction.undoCommand());
        //    cmd->redo();
        //}

    } else {
        KisTransformWorker worker(currentNode()->paintDevice(), m_currentArgs.scaleX(), m_currentArgs.scaleY(), m_currentArgs.shearX(), m_currentArgs.shearY(), m_currentArgs.aZ(), int(t.x()), int(t.y()), progress, m_filter);
        worker.run();
    }

	//canvas update
	QRect previousRect = QRect( m_previousTopLeft, m_previousBottomRight );

	QRectF currRectF = boundRect(m_topLeftProj, m_topRightProj, m_bottomRightProj, m_bottomLeftProj);
	QRect currRect = QRect( QPoint(currRectF.left(), currRectF.top()), QPoint(currRectF.right(), currRectF.bottom()) );
	currRect.adjust(- m_handleRadius - 1, - m_handleRadius - 1, m_handleRadius + 1, m_handleRadius + 1);

	QRect tmp = previousRect.united(currRect);

    currentNode()->setDirty(previousRect.united(currRect));

	//update previous points for later
	m_previousTopLeft = currRect.topLeft();
	m_previousBottomRight = currRect.bottomRight();

    canvas->view()->selectionManager()->selectionChanged();

    if (currentSelection() && currentSelection()->hasShapeSelection())
        canvas->view()->selectionManager()->shapeSelectionChanged();

    //// Else add the command -- this will have the memento from the previous state,
    //// and the transformed state from the original device we cached in our activated()
    //// method.
    transaction.commit(image()->undoAdapter());
    updater->deleteLater();
}
void KisToolTransform::notifyCommandAdded(const QUndoCommand * command)
{
    const ApplyTransformCmdData * cmd1 = dynamic_cast<const ApplyTransformCmdData*>(command);
    const TransformCmd* cmd2 = dynamic_cast<const TransformCmd*>(command);

    if (cmd1 == 0 && cmd2 == 0) {
        // The last added command wasn't one of ours;
        // we should reset to the new state of the canvas.
        // In effect we should treat this as if the tool has been just activated
        initHandles();
    }
}

void KisToolTransform::notifyCommandExecuted(const QUndoCommand * command)
{
    Q_UNUSED(command);
    const ApplyTransformCmdData * presentCmd1 = 0;
    const TransformCmd * presentCmd2 = 0;

	presentCmd1 = dynamic_cast<const ApplyTransformCmdData*>(image()->undoAdapter()->presentCommand());
	presentCmd2 = dynamic_cast<const TransformCmd*>(image()->undoAdapter()->presentCommand());

    if (presentCmd1 == 0 && presentCmd2 == 0) {
		updateCurrentOutline();
        // The command now on the top of the stack isn't one of ours
        // We should treat this as if the tool has been just activated
        initHandles();

		setButtonBoxDisabled(true);
    } else {
		if (presentCmd1 != 0) {
			//we have undone a transformation just after an "apply transformation" : we just to to reinit the handles
			initHandles();

			setButtonBoxDisabled(true);
		} else {
			//the present command (on top of a stack) is a simple transform : we ask for its arguments
			presentCmd2->transformArgs(m_currentArgs);
			m_origSelection = presentCmd2->origSelection(m_originalTopLeft, m_originalBottomRight);
			presentCmd2->origPreviews(m_origImg, m_origSelectionImg);

			setButtonBoxDisabled(false);
		}

		m_originalCenter = (m_originalTopLeft + m_originalBottomRight) / 2;
		m_previousTopLeft = m_originalTopLeft;
		m_previousBottomRight = m_originalBottomRight;
		m_scaleX_wOutModifier = m_currentArgs.scaleX();
		m_scaleY_wOutModifier = m_currentArgs.scaleY();
		m_refSize = QSizeF(0, 0); //will force the recalc of current QImages in recalcOutline

		updateOutlineChanged();
		refreshSpinBoxes();
    }
}

void KisToolTransform::slotSetFilter(const KoID &filterID)
{
    m_filter = KisFilterStrategyRegistry::instance()->value(filterID.id());
}

QWidget* KisToolTransform::createOptionWidget() {
    m_optWidget = new WdgToolTransform(0);
    Q_CHECK_PTR(m_optWidget);
    m_optWidget->setObjectName(toolId() + " option widget");

    m_optWidget->cmbFilter->clear();
    m_optWidget->cmbFilter->setIDList(KisFilterStrategyRegistry::instance()->listKeys());

    m_optWidget->cmbFilter->setCurrent("Bicubic");
    connect(m_optWidget->cmbFilter, SIGNAL(activated(const KoID &)),
            this, SLOT(slotSetFilter(const KoID &)));

    KoID filterID = m_optWidget->cmbFilter->currentItem();
    m_filter = KisFilterStrategyRegistry::instance()->value(filterID.id());

	m_rotCenterButtons = new QButtonGroup(0);
	//we set the ids to match m_handleDir
	m_rotCenterButtons->addButton(m_optWidget->middleRightButton, 0);
	m_rotCenterButtons->addButton(m_optWidget->topRightButton, 1);
	m_rotCenterButtons->addButton(m_optWidget->middleTopButton, 2);
	m_rotCenterButtons->addButton(m_optWidget->topLeftButton, 3);
	m_rotCenterButtons->addButton(m_optWidget->middleLeftButton, 4);
	m_rotCenterButtons->addButton(m_optWidget->bottomLeftButton, 5);
	m_rotCenterButtons->addButton(m_optWidget->middleBottomButton, 6);
	m_rotCenterButtons->addButton(m_optWidget->bottomRightButton, 7);
	m_rotCenterButtons->addButton(m_optWidget->centerButton, 8);

	
	QToolButton *auxButton = new QToolButton(0);
	auxButton->setCheckable(true);
	auxButton->setAutoExclusive(true);
	auxButton->hide(); //a convenient button for when no button is checked in the group
	m_rotCenterButtons->addButton(auxButton, 9);

	connect(m_rotCenterButtons, SIGNAL(buttonPressed(int)), this, SLOT(setRotCenter(int)));
	connect(m_optWidget->scaleXBox, SIGNAL(valueChanged(double)), this, SLOT(setScaleX(double)));
	connect(m_optWidget->scaleYBox, SIGNAL(valueChanged(double)), this, SLOT(setScaleY(double)));
	connect(m_optWidget->shearXBox, SIGNAL(valueChanged(double)), this, SLOT(setShearX(double)));
	connect(m_optWidget->shearYBox, SIGNAL(valueChanged(double)), this, SLOT(setShearY(double)));
	connect(m_optWidget->translateXBox, SIGNAL(valueChanged(double)), this, SLOT(setTranslateX(double)));
	connect(m_optWidget->translateYBox, SIGNAL(valueChanged(double)), this, SLOT(setTranslateY(double)));
	connect(m_optWidget->aXBox, SIGNAL(valueChanged(double)), this, SLOT(setAX(double)));
	connect(m_optWidget->aYBox, SIGNAL(valueChanged(double)), this, SLOT(setAY(double)));
	connect(m_optWidget->aZBox, SIGNAL(valueChanged(double)), this, SLOT(setAZ(double)));
	connect(m_optWidget->aspectButton, SIGNAL(keepAspectRatioChanged(bool)), this, SLOT(keepAspectRatioChanged(bool)));

	connect(m_optWidget->buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonBoxClicked(QAbstractButton *)));
	setButtonBoxDisabled(true);

	connect(m_optWidget->scaleXBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->scaleYBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->shearXBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->shearYBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->translateXBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->translateYBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->aXBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->aYBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));
	connect(m_optWidget->aZBox, SIGNAL(editingFinished()), this, SLOT(editingFinished()));

	refreshSpinBoxes();

	m_optWidget->warpButton->hide();
	//hide rotX
	m_optWidget->aXBox->hide();
	m_optWidget->label_8->hide();
	//hide rotY
	m_optWidget->aYBox->hide();
	m_optWidget->label_9->hide();

    return m_optWidget;
}

QWidget* KisToolTransform::optionWidget()
{
    return m_optWidget;
}

void KisToolTransform::setRotCenter(int id)
{
	if (!m_selecting) {
		if (id < 9) {
			double i = m_handleDir[id].x();
			double j = m_handleDir[id].y();

			m_currentArgs.setRotationCenterOffset(QPointF(i * m_originalWidth2, j * m_originalHeight2));
			updateOutlineChanged();

			m_boxValueChanged = true;
		}
	}
}

void KisToolTransform::setScaleX(double scaleX)
{
	if (!m_selecting) {
		//the spinbox has been modified directly
		m_currentArgs.setScaleX(scaleX / 100.);

		if (m_optWidget->aspectButton->keepAspectRatio() && m_optWidget->scaleXBox->value() != m_optWidget->scaleYBox->value())
			m_optWidget->scaleYBox->setValue(m_optWidget->scaleXBox->value());

		updateOutlineChanged();


		m_boxValueChanged = true;
	} else {
		//the scale factor has been modified by mouse movement : we set the aspect ratio button manually
		if (m_currentArgs.scaleX() == m_currentArgs.scaleY())
			m_optWidget->aspectButton->setKeepAspectRatio(true);
		else
			m_optWidget->aspectButton->setKeepAspectRatio(false);
	}
}

void KisToolTransform::setScaleY(double scaleY)
{
	if (!m_selecting) {
		//the spinbox has been modified directly
		m_currentArgs.setScaleY(scaleY / 100.);

		if (m_optWidget->aspectButton->keepAspectRatio() && m_optWidget->scaleXBox->value() != m_optWidget->scaleYBox->value())
			m_optWidget->scaleXBox->setValue(m_optWidget->scaleYBox->value());

		updateOutlineChanged();

		m_boxValueChanged = true;
	} else {
		//the scale factor has been modified by mouse movement : we set the aspect ratio button manually
		if (m_currentArgs.scaleX() == m_currentArgs.scaleY())
			m_optWidget->aspectButton->setKeepAspectRatio(true);
		else
			m_optWidget->aspectButton->setKeepAspectRatio(false);
	}
}

void KisToolTransform::setShearX(double shearX)
{
	if (!m_selecting) {
		m_currentArgs.setShearX(shearX);
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::setShearY(double shearY)
{
	if (!m_selecting) {
		m_currentArgs.setShearY(shearY);
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::setAX(double aX)
{
	if (!m_selecting) {
		m_currentArgs.setAX(degreeToRadian(aX));
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::setAY(double aY)
{
	if (!m_selecting) {
		m_currentArgs.setAY(degreeToRadian(aY));
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::setAZ(double aZ)
{
	if (!m_selecting) {
		m_currentArgs.setAZ(degreeToRadian(aZ));
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::setTranslateX(double translateX)
{
	if (!m_selecting) {
		m_currentArgs.setTranslate(QPointF(translateX, m_currentArgs.translate().y()));
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::setTranslateY(double translateY)
{
	if (!m_selecting) {
		m_currentArgs.setTranslate(QPointF(m_currentArgs.translate().x(), translateY));
		updateOutlineChanged();

		m_boxValueChanged = true;
	}
}

void KisToolTransform::buttonBoxClicked(QAbstractButton *button)
{
	if (m_optWidget == 0 || m_optWidget->buttonBox == 0)
		return;

	QAbstractButton *applyButton = m_optWidget->buttonBox->button(QDialogButtonBox::Apply);
	QAbstractButton *resetButton = m_optWidget->buttonBox->button(QDialogButtonBox::Reset);

	if (button == applyButton) {
		applyTransform();
		initHandles();

		setButtonBoxDisabled(true);
	} else if (button == resetButton) {
		initTransform();
		transform(); //commit the reset to the undo stack
		updateOutlineChanged();

		setButtonBoxDisabled(true);
	}
}

void KisToolTransform::keepAspectRatioChanged(bool keep)
{
	if (keep) {
		if (m_optWidget->scaleXBox->value() > m_optWidget->scaleYBox->value())
			m_optWidget->scaleYBox->setValue(m_optWidget->scaleXBox->value());
		else if (m_optWidget->scaleYBox->value() > m_optWidget->scaleXBox->value())
			m_optWidget->scaleXBox->setValue(m_optWidget->scaleYBox->value());
	}
}

void KisToolTransform::editingFinished()
{
	if (m_boxValueChanged) {
		updateCurrentOutline();

		transform();

		m_scaleX_wOutModifier = m_currentArgs.scaleX();
		m_scaleY_wOutModifier = m_currentArgs.scaleY();

		m_boxValueChanged = false;
	}
}

#include "kis_tool_transform.moc"
