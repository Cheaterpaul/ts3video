#ifndef TS3VIDEOCLIENT_P_H
#define TS3VIDEOCLIENT_P_H

#include <QHash>
#include <QPair>
#include <QUdpSocket>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QTime>

#include "cliententity.h"
#include "channelentity.h"
#include "networkusageentity.h"
#include "jsonprotocolhelper.h"

#include "vp8frame.h"
#include "yuvframe.h"

#include "ts3videoclient.h"
#include "udpvideoframedecoder.h"

class QCorConnection;
class MediaSocket;

class TS3VideoClientPrivate {
  Q_DISABLE_COPY(TS3VideoClientPrivate)
  Q_DECLARE_PUBLIC(TS3VideoClient)
  TS3VideoClientPrivate(TS3VideoClient*);
  TS3VideoClient * const q_ptr;

  QCorConnection *_connection;
  MediaSocket *_mediaSocket;
  ClientEntity _clientEntity;

  bool useMediaSocket; ///< TODO Remove me (DEV ONLY)
};

/*!
  Handles media data over UDP.
*/
class MediaSocket : public QUdpSocket
{
  Q_OBJECT

public:
  MediaSocket(const QString &token, QObject *parent);
  ~MediaSocket();
  bool isAuthenticated() const;
  void setAuthenticated(bool yesno);

  /*! Encodes and sends the image as new video frame of the current
      video stream to the server.
   */
  void sendVideoFrame(const QImage &image, int senderId);

signals:
  /*! Emits with every new arrived and decoded video frame.
   */
  void newVideoFrame(YuvFrameRefPtr frame, int senderId);

  /*! Emits periodically with newest calculated network-usage information.
   */
  void networkUsageUpdated(const NetworkUsageEntity &networkUsage);

protected:
  void sendAuthTokenDatagram(const QString &token);
  void sendVideoFrame(const QByteArray &frame, quint64 frameId, quint32 senderId);
  void sendVideoFrameRecoveryDatagram(quint64 frameId, quint32 fromSenderId);
  virtual void timerEvent(QTimerEvent *ev);

private slots:
  void onSocketStateChanged(QAbstractSocket::SocketState state);
  void onReadyRead();

private:
  bool _authenticated;
  QString _token;
  int _authenticationTimerId;

  // Encoding.
  class VideoEncodingThread *_videoEncodingThread;
  unsigned long long _lastFrameRequestTimestamp;

  // Decoding.
  QHash<int, VideoFrameUdpDecoder*> _videoFrameDatagramDecoders; ///< Maps client-id to it's decoder.
  class VideoDecodingThread *_videoDecodingThread;

  // Network usage.
  NetworkUsageEntity _networkUsage;
  QTime _bandwidthCalcTime;
  quint64 _bandwidthReadTemp;
  quint64 _bandwidthWrittenTemp;
};

/*!
  Encodes and serializes video frames.
 */
class VideoEncodingThread : public  QThread
{
  Q_OBJECT

public:
  VideoEncodingThread(QObject *parent);
  ~VideoEncodingThread();
  void stop();
  void enqueue(const QImage &image, int senderId);
  void enqueueRecovery();

protected:
  void run();

signals:
  void encoded(QByteArray &frame, int senderId);

private:
  QMutex _m;
  QWaitCondition _queueCond;
  QQueue<QPair<QImage, int> > _queue; ///< Replace with RingQueue (Might not keep more than X frames! Otherwise we might get a memory problem.)
  QAtomicInt _stopFlag;
  QAtomicInt _recoveryFlag;
};

/*!
*/
class VideoDecodingThread : public QThread
{
  Q_OBJECT

public:
  VideoDecodingThread(QObject *parent);
  ~VideoDecodingThread();
  void stop();
  void enqueue(VP8Frame *frame, int senderId);

protected:
  void run();

signals:
  void decoded(YuvFrameRefPtr frame, int senderId);

private:
  QMutex _m;
  QWaitCondition _queueCond;
  QQueue<QPair<VP8Frame*, int> > _queue;
  QAtomicInt _stopFlag;
};

#endif // TS3VIDEOCLIENT_P_H
