#include "clientconnectionhandler.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTcpSocket>

#include "humblelogging/api.h"

#include "qcorconnection.h"
#include "qcorreply.h"

#include "cliententity.h"
#include "channelentity.h"
#include "jsonprotocolhelper.h"

#include "ts3videoserver.h"

HUMBLE_LOGGER(HL, "server.clientconnection");

///////////////////////////////////////////////////////////////////////

ClientConnectionHandler::ClientConnectionHandler(TS3VideoServer *server, QCorConnection *connection, QObject *parent) :
  QObject(parent),
  _server(server),
  _connection(connection),
  _authenticated(false),
  _clientEntity(new ClientEntity())
{
  _clientEntity = new ClientEntity();
  _clientEntity->id = ++_server->_nextClientId;
  _server->_clients.insert(_clientEntity->id, _clientEntity);
  
  _server->_connections.insert(_clientEntity->id, this);
  connect(_connection, &QCorConnection::stateChanged, this, &ClientConnectionHandler::onStateChanged);
  connect(_connection, &QCorConnection::newIncomingRequest, this, &ClientConnectionHandler::onNewIncomingRequest);

  onStateChanged(QAbstractSocket::ConnectedState);
}

ClientConnectionHandler::~ClientConnectionHandler()
{
  _server->_clients.remove(_clientEntity->id);
  _server->_connections.remove(_clientEntity->id);
  delete _clientEntity;
  delete _connection;
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

  QString action;
  QJsonObject params;
  if (!JsonProtocolHelper::fromJsonRequest(frame->data(), action, params)) {
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponseError(500, QString("Invalid protocol format")));
    _connection->sendResponse(res);
    return;
  }

  if (action == "auth") {
    auto version = params["version"].toInt();
    auto username = params["username"].toString();
    // Compare client version against server version compatibility.
    if (version != TS3VIDEOSERVER_VERSION) {
      QCorFrame res;
      res.initResponse(*frame.data());
      res.setData(JsonProtocolHelper::createJsonResponseError(3, QString("Incompatible version (client=%1; server=%2)").arg(version).arg(TS3VIDEOSERVER_VERSION)));
      _connection->sendResponse(res);
      return;
    }
    // Authenticate.
    if (username.isEmpty()) {
      QCorFrame res;
      res.initResponse(*frame.data());
      res.setData(JsonProtocolHelper::createJsonResponseError(4, QString("Authentication failed")));
      _connection->sendResponse(res);
      return;
    }
    _authenticated = true;
    _clientEntity->name = username;
    auto token = QString("%1-%2").arg(_clientEntity->id).arg(QDateTime::currentDateTimeUtc().toString());
    _server->_tokens.insert(token, _clientEntity->id);
    // Send response.
    QJsonObject resData;
    resData["client"] = _clientEntity->toQJsonObject();
    resData["authtoken"] = token;
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponse(resData));
    _connection->sendResponse(res);
    return;
  }
  else if (action == "goodbye") {
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponse(QJsonObject()));
    _connection->sendResponse(res);
    _connection->disconnectFromHost();
    return;
  }

  // The client needs to be authenticated before he can request any other actions.
  // Close connection, if the client tries anything else.
  if (!_authenticated) {
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponseError(4, QString("Authentication failed")));
    _connection->sendResponse(res);
    _connection->disconnectFromHost();
    return;
  }

  if (action == "joinchannel") {
    auto channelId = params["channelid"].toInt();
    if (channelId <= 0) {
      // Send error: Missing channel id.
      QCorFrame res;
      res.initResponse(*frame.data());
      res.setData(JsonProtocolHelper::createJsonResponseError(1, QString("Invalid channel id (channelid=%1)").arg(channelId)));
      _connection->sendResponse(res);
      return;
    }
    // Create channel.
    auto channelEntity = _server->_channels.value(channelId);
    if (!channelEntity) {
      channelEntity = new ChannelEntity();
      channelEntity->id = channelId; //++_server->_nextChannelId;
      _server->_channels.insert(channelEntity->id, channelEntity);
    }
    // Join channel.
    _server->_participants[channelEntity->id].insert(_clientEntity->id);
    // Send response.
    auto participants = _server->_participants[channelEntity->id];
    QJsonObject params;
    params["channel"] = channelEntity->toQJsonObject();
    QJsonArray paramsParticipants;
    foreach (auto clientId, participants) {
      auto client = _server->_clients.value(clientId);
      if (client) {
        paramsParticipants.append(client->toQJsonObject());
      }
    }
    params["participants"] = paramsParticipants;
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponse(params));
    _connection->sendResponse(res);
    // Notify participants about the new client.
    params = QJsonObject();
    params["channel"] = channelEntity->toQJsonObject();
    params["client"] = _clientEntity->toQJsonObject();
    QCorFrame req;
    req.setData(JsonProtocolHelper::createJsonRequest("notify.clientjoinedchannel", params));
    foreach (auto clientId, participants) {
      auto conn = _server->_connections.value(clientId);
      if (conn && conn != this) {
        auto reply = conn->_connection->sendRequest(req);
        connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
      }
    }
    return;
  }
  else if (action == "leavechannel") {
    auto channelId = params["channelid"].toInt();
    // Find channel.
    auto channelEntity = _server->_channels.value(channelId);
    if (!channelEntity) {
      // Send error: Invalid channel id.
      QCorFrame res;
      res.initResponse(*frame.data());
      res.setData(JsonProtocolHelper::createJsonResponseError(1, QString("Invalid channel id (channelid=%1)").arg(channelId)));
      _connection->sendResponse(res);
      return;
    }
    // Leave channel.
    _server->_participants[channelEntity->id].remove(_clientEntity->id);
    // Send response.
    QCorFrame res;
    res.initResponse(*frame.data());
    res.setData(JsonProtocolHelper::createJsonResponse(QJsonObject()));
    _connection->sendResponse(res);
    // Notify participants.
    params = QJsonObject();
    params["channel"] = channelEntity->toQJsonObject();
    params["client"] = _clientEntity->toQJsonObject();
    QCorFrame req;
    req.setData(JsonProtocolHelper::createJsonRequest("notify.clientleftchannel", params));
    auto participants = _server->_participants[channelEntity->id];
    foreach(auto clientId, participants) {
      auto conn = _server->_connections.value(clientId);
      if (conn && conn != this) {
        auto reply = conn->_connection->sendRequest(req);
        connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
      }
    }
    // Delete channel.
    if (_server->_participants[channelEntity->id].isEmpty()) {
      _server->_participants.remove(channelEntity->id);
      _server->_channels.remove(channelEntity->id);
      delete channelEntity;
    }
    return;
  }

  QCorFrame res;
  res.initResponse(*frame.data());
  res.setData(JsonProtocolHelper::createJsonResponseError(4, QString("Unknown action.")));
  _connection->sendResponse(res);
  return;
}