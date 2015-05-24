#include "clientconnectionhandler.h"

#include <QDateTime>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTcpSocket>

#include "humblelogging/api.h"

#include "qcorconnection.h"
#include "qcorreply.h"

#include "ts3video.h"
#include "elws.h"
#include "jsonprotocolhelper.h"

#include "virtualserver.h"
#include "virtualserver_p.h"
#include "servercliententity.h"
#include "serverchannelentity.h"

HUMBLE_LOGGER(HL, "server.clientconnection");

///////////////////////////////////////////////////////////////////////

ClientConnectionHandler::ClientConnectionHandler(VirtualServer *server, QCorConnection *connection, QObject *parent) :
  QObject(parent),
  _server(server),
  _connection(connection),
  _authenticated(false),
  _isAdmin(false),
  _clientEntity(nullptr),
  _networkUsage(),
  _networkUsageHelper(_networkUsage)
{
  _clientEntity = new ServerClientEntity();
  _clientEntity->id = ++_server->_nextClientId;
  _server->_clients.insert(_clientEntity->id, _clientEntity);

  _server->_connections.insert(_clientEntity->id, this);
  connect(_connection, &QCorConnection::stateChanged, this, &ClientConnectionHandler::onStateChanged);
  connect(_connection, &QCorConnection::newIncomingRequest, this, &ClientConnectionHandler::onNewIncomingRequest);
  onStateChanged(QAbstractSocket::ConnectedState);

  // Authentication timeout.
  // Close connection if it doesn't authentication within X seconds.
  QTimer::singleShot(5000, [this] () {
    if (!_connection)
      return;
    if (!_authenticated) {
      HL_WARN(HL, QString("Client did not authenticate within X seconds. Dropping connection...").toStdString());
      QCorFrame req;
      QJsonObject params;
      params["code"] = 1;
      params["message"] = "Authentication timed out.";
      req.setData(JsonProtocolHelper::createJsonRequest("error", params));
      auto reply = _connection->sendRequest(req);
      QObject::connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
      QMetaObject::invokeMethod(_connection, "disconnectFromHost", Qt::QueuedConnection);
    }
  });

  // Connection timeout.
  _connectionTimeoutTimer.start(20000);
  QObject::connect(&_connectionTimeoutTimer, &QTimer::timeout, [this] () {
    if (!_connection)
      return;
    HL_WARN(HL, QString("Client connection timed out. No heartbeat since 20 seconds.").toStdString());
    QCorFrame req;
    QJsonObject params;
    params["code"] = 1;
    params["message"] = "Connection timed out.";
    req.setData(JsonProtocolHelper::createJsonRequest("error", params));
    auto reply = _connection->sendRequest(req);
    QObject::connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
    QMetaObject::invokeMethod(_connection, "disconnectFromHost", Qt::QueuedConnection);
  });

  // Prepare statistics.
  auto statisticTimer = new QTimer(this);
  statisticTimer->setInterval(1500);
  statisticTimer->start();
  connect(statisticTimer, &QTimer::timeout, [this] () {
    _networkUsageHelper.recalculate();
    emit networkUsageUpdated(_networkUsage);
  });

  // Max number of connections (Connection limit).
  if (_server->_connections.size() > _server->_opts.connectionLimit) {
    HL_WARN(HL, QString("Server connection limit exceeded. (max=%1)").arg(_server->_opts.connectionLimit).toStdString());
    QCorFrame req;
    QJsonObject params;
    params["code"] = 1;
    params["message"] = "Server connection limit exceeded.";
    req.setData(JsonProtocolHelper::createJsonRequest("error", params));
    auto reply = _connection->sendRequest(req);
    connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
    QMetaObject::invokeMethod(_connection, "disconnectFromHost", Qt::QueuedConnection);
  }

  // Max bandwidth usage (Bandwidth limit).
  if (_server->_networkUsageMediaSocket.bandwidthRead > _server->_opts.bandwidthReadLimit || _server->_networkUsageMediaSocket.bandwidthWrite > _server->_opts.bandwidthWriteLimit) {
    HL_WARN(HL, QString("Server bandwidth limit exceeded.").toStdString());
    QCorFrame req;
    QJsonObject params;
    params["code"] = 1;
    params["message"] = "Server bandwidth limit exceeded.";
    req.setData(JsonProtocolHelper::createJsonRequest("error", params));
    auto reply = _connection->sendRequest(req);
    connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
    QMetaObject::invokeMethod(_connection, "disconnectFromHost", Qt::QueuedConnection);
  }
}

ClientConnectionHandler::~ClientConnectionHandler()
{
  _server->removeClientFromChannels(_clientEntity->id);
  _server->_clients.remove(_clientEntity->id);
  _server->_connections.remove(_clientEntity->id);
  delete _clientEntity;
  delete _connection;
  _clientEntity = nullptr;
  _connection = nullptr;
  _server = nullptr;
}

void ClientConnectionHandler::sendMediaAuthSuccessNotify()
{
  QCorFrame req;
  req.setData(JsonProtocolHelper::createJsonRequest("notify.mediaauthsuccess", QJsonObject()));
  auto reply = _connection->sendRequest(req);
  connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
}

void ClientConnectionHandler::onStateChanged(QAbstractSocket::SocketState state)
{
  switch (state) {
    case QAbstractSocket::ConnectedState: {
      HL_INFO(HL, QString("New client connection (addr=%1; port=%2; clientid=%3)").arg(_connection->socket()->peerAddress().toString()).arg(_connection->socket()->peerPort()).arg(_clientEntity->id).toStdString());
      break;
    }
    case QAbstractSocket::UnconnectedState: {
      HL_INFO(HL, QString("Client disconnected (addr=%1; port=%2; clientid=%3)").arg(_connection->socket()->peerAddress().toString()).arg(_connection->socket()->peerPort()).arg(_clientEntity->id).toStdString());
      // Notify sibling clients about the disconnect.
      QJsonObject params;
      params["client"] = _clientEntity->toQJsonObject();
      QCorFrame req;
      req.setData(JsonProtocolHelper::createJsonRequest("notify.clientdisconnected", params));
      auto clientConns = _server->_connections.values();
      foreach(auto clientConn, clientConns) {
        if (clientConn && clientConn != this) {
          auto reply = clientConn->_connection->sendRequest(req);
          if (reply) connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
        }
      }
      // Delete itself.
      deleteLater();
      break;
    }
  }
}

void ClientConnectionHandler::onNewIncomingRequest(QCorFrameRefPtr frame)
{
  HL_TRACE(HL, QString("New incoming request (size=%1; content=%2)").arg(frame->data().size()).arg(QString(frame->data())).toStdString());
  _networkUsage.bytesRead += frame->data().size(); // TODO Not correct, we need to get values from QCORLIB to include bytes of cor_frame (same for write).

  // Parse incoming request.
  QString action;
  QJsonObject params;
  if (!JsonProtocolHelper::fromJsonRequest(frame->data(), action, params)) {
    HL_WARN(HL, QString("Retrieved request with invalid JSON protocol format.").toStdString());
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponseError(500, QString("Invalid protocol format")));
    _connection->sendResponse(res);
    return;
  }

  // Find matching action handler.
  // TODO If the client sends more than X invalid action requests, we should disconnect him.
  auto &serverData = _server->d;
  auto actionHandler = serverData->actions.value(action);
  if (!actionHandler) {
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponseError(4, QString("Unknown action.")));
    _connection->sendResponse(res);
    return;
  }

  // The client needs to be authenticated before he can request any action with RequiresAuthentication flag.
  // Close connection, if the client tries anything else.
  if (actionHandler->flags().testFlag(ActionBase::RequiresAuthentication) && !_authenticated) {
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponseError(4, QString("Authentication failed")));
    _connection->sendResponse(res);
    _connection->disconnectFromHost();
    return;
  }

  // The request and its prerequisites seems to be legit
  //   -> process it.
  ActionData req;
  req.server = _server;
  req.session = this;
  req.connection = _connection;
  req.frame = frame;
  req.action = action;
  req.params = params;

  actionHandler->_req = req;
  actionHandler->run();
}
