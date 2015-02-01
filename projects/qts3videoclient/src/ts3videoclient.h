#ifndef TS3VIDEOCLIENT_H
#define TS3VIDEOCLIENT_H

#include <QObject>
#include <QScopedPointer>
#include <QAbstractSocket>

#include "qcorframe.h"

class QHostAddress;
class QCorReply;
class ClientEntity;
class ChannelEntity;

class TS3VideoClientPrivate;
class TS3VideoClient : public QObject
{
  Q_OBJECT
  Q_DECLARE_PRIVATE(TS3VideoClient)
  QScopedPointer<TS3VideoClientPrivate> const d_ptr;

public:
  TS3VideoClient(QObject *parent = 0);
  TS3VideoClient(const TS3VideoClient &other);
  ~TS3VideoClient();

  const ClientEntity& clientEntity() const;
  bool isReadyForStreaming() const;

  void connectToHost(const QHostAddress &address, qint16 port);
  QCorReply* auth(const QString &name);
  QCorReply* joinChannel();

  /*!
    Sends a single frame to the server, which will then broadcast it to other clients.
    Internally encodes the image with VPX codec.
    \thread-safe
    \param image A single frame of the video.
  */
  void sendVideoFrame(const QImage &image);

signals:
  void connected();
  void disconnected();
  void clientJoinedChannel(const ClientEntity &client, const ChannelEntity &channel);
  void clientLeftChannel(const ClientEntity &client, const ChannelEntity &channel);
  void clientDisconnected(const ClientEntity &client);
  void newVideoFrame(const QImage &image, int senderId);

private slots:
  void onStateChanged(QAbstractSocket::SocketState state);
  void onNewIncomingRequest(QCorFrameRefPtr frame);
};

#endif // TS3VIDEOCLIENT_H
