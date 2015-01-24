#ifndef TS3VIDEOCLIENT_H
#define TS3VIDEOCLIENT_H

#include <QObject>
#include <QScopedPointer>
#include <QHostAddress>

#include "qcorframe.h"

class QCorReply;

class TS3VideoClientPrivate;
class TS3VideoClient : public QObject
{
  Q_OBJECT
  Q_DECLARE_PRIVATE(TS3VideoClient)
  QScopedPointer<TS3VideoClientPrivate> const d_ptr;

public:
  explicit TS3VideoClient(QObject *parent = 0);
  ~TS3VideoClient();
  void connectToHost(const QHostAddress &address, qint16 port);
  QCorReply* auth();

private slots:
  void onStateChanged(QAbstractSocket::SocketState state);
  void onNewIncomingRequest(QCorFrameRefPtr frame);
};

#endif // TS3VIDEOCLIENT_H
