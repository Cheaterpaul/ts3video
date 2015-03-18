#ifndef WEBSOCKETSTATUSSERVER_H
#define WEBSOCKETSTATUSSERVER_H

#include <QObject>
#include <QList>

class QWebSocketServer;
class QWebSocket;
class TS3VideoServer;

class WebSocketStatusServer : public QObject
{
  Q_OBJECT

public:
  WebSocketStatusServer(TS3VideoServer *server);
  virtual ~WebSocketStatusServer();
  bool init();

signals:
  void closed();

private slots:
  void onNewConnection();
  void onTextMessage(const QString &message);
  void onDisconnected();
  
private:
  QJsonValue getAppInfo() const;
  QJsonValue getMemoryUsageInfo() const;
  QJsonValue getBandwidthInfo() const;
  QJsonValue getClientsInfo() const;
  QJsonValue getChannelsInfo() const;
  QJsonValue getWebSocketsInfo() const;

private:
  TS3VideoServer *_server;
  QWebSocketServer *_wsServer;
  QList<QWebSocket*> _sockets;
};

#endif