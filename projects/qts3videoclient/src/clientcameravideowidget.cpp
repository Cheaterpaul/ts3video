#include "clientcameravideowidget.h"

#include <QDebug>
#include <QBoxLayout>
#include <QLabel>

#include <QCameraInfo>

#include <QVideoWidget>

#include "clientvideowidget.h"

///////////////////////////////////////////////////////////////////////

ClientCameraVideoWidget::ClientCameraVideoWidget(QWidget *parent) :
  QWidget(parent)
{
  auto cameraInfo = QCameraInfo::defaultCamera();
  auto camera = new QCamera(cameraInfo, this);
  camera->start();

  auto videoWidget = new QVideoWidget();
  videoWidget->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
  videoWidget->setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
  camera->setViewfinder(videoWidget);

  //auto grabber = new CameraFrameGrabber(this);
  //auto videoWidget = new ClientVideoWidget();
  //QObject::connect(grabber, &CameraFrameGrabber::newFrame, videoWidget, &ClientVideoWidget::setImage);
  //QObject::connect(grabber, &CameraFrameGrabber::newPixmapFrame, videoWidget, &ClientVideoWidget::setFrame);

  auto cameraLabel = new QLabel();
  cameraLabel->setText(tr("Camera: %1").arg(cameraInfo.description()));

  auto mainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->addWidget(videoWidget, 1);
  mainLayout->addWidget(cameraLabel, 0);
  setLayout(mainLayout);
}

ClientCameraVideoWidget::~ClientCameraVideoWidget()
{

}

///////////////////////////////////////////////////////////////////////

CameraFrameGrabber::CameraFrameGrabber(QObject *parent) :
  QAbstractVideoSurface(parent)
{

}

QList<QVideoFrame::PixelFormat> CameraFrameGrabber::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
  static QList<QVideoFrame::PixelFormat> __formats;
  if (__formats.isEmpty()) {
    __formats
      << QVideoFrame::Format_ARGB32
      << QVideoFrame::Format_ARGB32_Premultiplied
      << QVideoFrame::Format_RGB32
      << QVideoFrame::Format_RGB24
      << QVideoFrame::Format_RGB565
      << QVideoFrame::Format_RGB555
      << QVideoFrame::Format_ARGB8565_Premultiplied
      << QVideoFrame::Format_BGRA32
      << QVideoFrame::Format_BGRA32_Premultiplied
      << QVideoFrame::Format_BGR32
      << QVideoFrame::Format_BGR24
      << QVideoFrame::Format_BGR565
      << QVideoFrame::Format_BGR555
      << QVideoFrame::Format_BGRA5658_Premultiplied
      << QVideoFrame::Format_AYUV444
      << QVideoFrame::Format_AYUV444_Premultiplied
      << QVideoFrame::Format_YUV444
      << QVideoFrame::Format_YUV420P
      << QVideoFrame::Format_YV12
      << QVideoFrame::Format_UYVY
      << QVideoFrame::Format_YUYV
      << QVideoFrame::Format_NV12
      << QVideoFrame::Format_NV21
      << QVideoFrame::Format_IMC1
      << QVideoFrame::Format_IMC2
      << QVideoFrame::Format_IMC3
      << QVideoFrame::Format_IMC4
      << QVideoFrame::Format_Y8
      << QVideoFrame::Format_Y16
      << QVideoFrame::Format_Jpeg
      << QVideoFrame::Format_CameraRaw
      << QVideoFrame::Format_AdobeDng;
  }
  return __formats;
}

bool CameraFrameGrabber::present(const QVideoFrame &frame)
{
  if (!frame.isValid()) {
    qDebug() << QString("Invalid video frame.");
    return false;
  }
  // Clone frame to have it as non-const.
  QVideoFrame f(frame);
  // Convert video frame to QImage.
  auto imageFormat = QVideoFrame::imageFormatFromPixelFormat(f.pixelFormat());
  if (imageFormat == QImage::Format_Invalid) {
    qDebug() << QString("Invalid image format for video frame.");
    return false;
  }
  // Calls the deep copy constructor of QImage.
  f.map(QAbstractVideoBuffer::ReadOnly);

  const QImage image(f.bits(), f.width(), f.height(), imageFormat);
  emit newFrame(image);

  //auto pm = QPixmap::fromImage(image);
  //emit newPixmapFrame(pm);

  f.unmap();
  return true;
}
