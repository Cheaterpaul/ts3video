#include "hangoutviewwidget_p.h"

#include <QDebug>
#include <QTimer>
#include <QBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizeGrip>
#include <QResizeEvent>
#include <QGraphicsDropShadowEffect>

#include "cliententity.h"

#include "remoteclientvideowidget.h"

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

HangoutViewWidget::HangoutViewWidget(QWidget *parent) :
  QWidget(parent),
  d(new HangoutViewWidgetPrivate(this))
{
  auto pal = palette();
  d->thumbnailWidgetSize.scale(250, 250, Qt::KeepAspectRatio);

  // Full view area.
  d->fullViewContainer = new QWidget(this);
  d->fullViewContainer->setObjectName("fullViewContainer");
  //d->fullViewContainer->setStyleSheet("#fullViewContainer { background: qlineargradient(spread:pad, x1:0.514, y1:0, x2:1, y2:1, stop:0 rgba(0, 0, 81, 255), stop:1 rgba(0, 0, 25, 255)); }");

  d->fullViewWidget = new HangoutViewFullViewWidget(d->fullViewContainer);

  // Camera view.
  d->cameraWidget = new HangoutViewCameraWidget(this);

  // Thumbnail area.
  d->thumbnailContainer = new QWidget(this);
  pal = d->thumbnailContainer->palette();
  pal.setColor(QPalette::Background, Qt::lightGray);
  d->thumbnailContainer->setPalette(pal);

  d->thumbnailContainerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
  d->thumbnailContainerLayout->setContentsMargins(0, 0, 0, 0);
  d->thumbnailContainerLayout->addStretch(1);
  d->thumbnailContainerLayout->addStretch(1);
  d->thumbnailContainer->setLayout(d->thumbnailContainerLayout);

  d->thumbnailScrollArea = new QScrollArea(this);
  d->thumbnailScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  d->thumbnailScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  d->thumbnailScrollArea->setWidgetResizable(true);
  d->thumbnailScrollArea->setFrameShape(QFrame::NoFrame);
  d->thumbnailScrollArea->setFrameShadow(QFrame::Plain);
  d->thumbnailScrollArea->setWidget(d->thumbnailContainer);
  d->thumbnailScrollArea->setMinimumHeight(d->thumbnailWidgetSize.height() + d->thumbnailScrollArea->horizontalScrollBar()->height() + 9);

  // Layout.
  auto mainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  mainLayout->addWidget(d->fullViewContainer, 1);
  mainLayout->addWidget(d->thumbnailScrollArea, 0);
  setLayout(mainLayout);

  resize(800, 600);
  setVisible(true);
}

HangoutViewWidget::~HangoutViewWidget()
{
}

void HangoutViewWidget::setCameraWidget(QWidget *w)
{
  d->cameraWidget->setWidget(w);
  d->doCameraLayout();
}

void HangoutViewWidget::addClient(const ClientEntity &client, const ChannelEntity &channel)
{
  // Create thumbnail widget.
  auto hw = new HangoutViewThumbnailWidget(client, d->thumbnailContainer);
  hw->setFixedSize(d->thumbnailWidgetSize);

  // Add to thumbnail area.
  d->thumbnailContainerLayout->insertWidget(d->thumbnailContainerLayout->count() - 1, hw);

  // Mappings.
  d->thumbnailWidgets.insert(client.id, hw);
  d->videoWidgets.insert(client.id, hw->remoteVideoWidget());

  QObject::connect(hw, &HangoutViewThumbnailWidget::clicked, [this, client] () -> void {
    qDebug() << QString("Clicked %1").arg(client.id);
    d->fullViewClientId = client.id;
  });

  // Create full-view video-widget.
  if (d->fullViewClientId == -1) {
    d->fullViewClientId = client.id;
  }
}

void HangoutViewWidget::removeClient(const ClientEntity &client, const ChannelEntity &channel)
{
  auto hw = d->thumbnailWidgets.value(client.id);
  if (!hw)
    return;

  // Mappings.
  d->thumbnailWidgets.remove(client.id);
  d->videoWidgets.remove(client.id);

  // Remove from layout and delete it.
  hw->setVisible(false);
  d->thumbnailContainerLayout->removeWidget(hw);
  delete hw;
}

void HangoutViewWidget::updateClientVideo(YuvFrameRefPtr frame, int senderId)
{
  auto videoWidget = d->videoWidgets.value(senderId);
  if (!videoWidget) {
    //HL_WARN(HL, QString("Received video frame for unknown client (client-id=%1)").arg(senderId).toStdString());
    return;
  }
  if (senderId == d->fullViewClientId) {
    auto w = d->fullViewWidget->remoteVideoWidget();
    if (w) {
      w->videoWidget()->setFrame(frame);
    }
  }
  videoWidget->videoWidget()->setFrame(frame);
}

void HangoutViewWidget::resizeEvent(QResizeEvent *ev)
{
  d->doFullViewVideoLayout();
  d->doCameraLayout();
}

void HangoutViewWidgetPrivate::doFullViewVideoLayout()
{
  auto d = this;
  if (!d->fullViewWidget)
    return;

  auto spacing = 9;
  auto rect = d->fullViewContainer->rect();
  auto widgetPos = QPoint(0, 0);
  auto widgetSize = QSize(4, 3);

  // Calculate widget size and position.
  const auto mode = Qt::KeepAspectRatio;
  switch (mode) {
    case Qt::KeepAspectRatio:
      widgetSize.scale(rect.size(), Qt::KeepAspectRatio);
      if (widgetSize.width() < rect.width()) {
        widgetPos += QPoint((rect.width() - widgetSize.width()) / 2, 0);
      }
      if (widgetSize.height() < rect.height()) {
        widgetPos += QPoint(0, (rect.height() - widgetSize.height()) / 2);
      }
      if (spacing > 0) {
        widgetSize.setWidth(widgetSize.width() - spacing * 2);
        widgetSize.setHeight(widgetSize.height() - spacing * 2);
        widgetPos += QPoint(spacing, spacing);
      }
      break;
    default:
      widgetSize = rect.size();
      break;
  }

  d->fullViewWidget->move(widgetPos);
  d->fullViewWidget->resize(widgetSize);
  d->fullViewWidget->setVisible(true);
}

void HangoutViewWidgetPrivate::doCameraLayout()
{
  auto d = this;
  if (!d->cameraWidget)
    return;

  const auto spacing = 9;

  //d->cameraWidget->resize(cameraWidgetSize);

  auto pos = d->fullViewContainer->rect().bottomRight();
  pos.setX(pos.x() - spacing - d->cameraWidget->width());
  pos.setY(pos.y() - spacing - d->cameraWidget->height());
  d->cameraWidget->move(pos);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

HangoutViewFullViewWidget::HangoutViewFullViewWidget(QWidget *parent) :
  QFrame(parent),
  _videoWidget(nullptr)
{
  auto pal = palette();
  pal.setColor(QPalette::Background, Qt::white);
  setPalette(pal);

  setAutoFillBackground(true);
  setFrameShape(QFrame::StyledPanel);

  auto dropShadow = new QGraphicsDropShadowEffect(this);
  dropShadow->setOffset(0, 0);
  dropShadow->setBlurRadius(5);
  setGraphicsEffect(dropShadow);

  QTimer::singleShot(1, this, SLOT(createRemoteVideoWidget()));
}

void HangoutViewFullViewWidget::createRemoteVideoWidget()
{
  // Content.
  _videoWidget = ViewBase::createRemoteVideoWidget(ClientEntity(), this);

  // Layout.
  auto l = new QBoxLayout(QBoxLayout::TopToBottom);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(0);
  l->addWidget(_videoWidget);
  setLayout(l);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

HangoutViewCameraWidget::HangoutViewCameraWidget(QWidget *parent) :
  QFrame(parent),
  _minSize(160, 113),
  _widget(nullptr)
{
  setMouseTracking(true);
  setAutoFillBackground(true);
  setFrameShape(QFrame::StyledPanel);
  setWindowFlags(Qt::SubWindow);
  setMinimumSize(_minSize);

  _sizeGrip = new QSizeGrip(this);
}

HangoutViewCameraWidget::~HangoutViewCameraWidget()
{
  if (_widget) {
    _widget->setVisible(false);
    _widget->setParent(nullptr);
  }
}

void HangoutViewCameraWidget::setWidget(QWidget *w)
{
  if (_widget) {
    _widget->setParent(nullptr);
    _widget->setVisible(false);
  }
  _widget = w;
  _widget->setParent(this);
  _widget->setVisible(true);

  auto l = new QBoxLayout(QBoxLayout::TopToBottom);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(0);
  l->addWidget(_widget);
  setLayout(l);

  _widget->stackUnder(_sizeGrip);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

HangoutViewThumbnailWidget::HangoutViewThumbnailWidget(const ClientEntity &client, QWidget *parent) :
  QFrame(parent),
  _mouseDown(false),
  _videoWidget(nullptr)
{
  setMouseTracking(true);
  setAutoFillBackground(true);
  setFrameShape(QFrame::StyledPanel);

  auto dropShadow = new QGraphicsDropShadowEffect(this);
  dropShadow->setOffset(0, 0);
  dropShadow->setBlurRadius(5);
  setGraphicsEffect(dropShadow);

  // Content.
  _videoWidget = ViewBase::createRemoteVideoWidget(client, this);

  // Layout.
  auto l = new QBoxLayout(QBoxLayout::TopToBottom);
  l->setContentsMargins(0, 0, 0, 0);
  l->setSpacing(0);
  l->addWidget(_videoWidget);
  setLayout(l);
}

void HangoutViewThumbnailWidget::mousePressEvent(QMouseEvent *ev)
{
  _mouseDown = true;
}

void HangoutViewThumbnailWidget::mouseReleaseEvent(QMouseEvent *ev)
{
  if (_mouseDown) {
    _mouseDown = false;
    emit clicked();
  }
}