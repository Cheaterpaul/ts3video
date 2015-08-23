#include "actionbase.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTcpSocket>
#include <QtConcurrent>

#include "humblelogging/api.h"

#include "qcorconnection.h"
#include "qcorreply.h"

#include "ts3util.h"
#include "ts3serverquery.h"
#include "ts3queryconsolesocket.h"

#include "qtasync.h"

#include "ts3video.h"
#include "elws.h"
#include "jsonprotocolhelper.h"

#include "../virtualserver_p.h"
#include "../clientconnectionhandler.h"
#include "../servercliententity.h"
#include "../serverchannelentity.h"

HUMBLE_LOGGER(HL, "server.clientconnection.action");

///////////////////////////////////////////////////////////////////////

void ActionBase::sendErrorRequest(QCorConnection& con, int code, const QString& message)
{
	QCorFrame r;
	QJsonObject p;
	p["code"] = code;
	p["message"] = message;
	r.setData(JsonProtocolHelper::createJsonRequest("error", p));
	auto reply = con.sendRequest(r);
	if (reply) QObject::connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
}

void ActionBase::sendOkResponse(QCorConnection& con, const QCorFrame& req, const QJsonObject& params)
{
	QCorFrame res;
	res.initResponse(req);
	res.setData(JsonProtocolHelper::createJsonResponse(params));
	con.sendResponse(res);
}

void ActionBase::sendErrorResponse(QCorConnection& con, const QCorFrame& req, int code, const QString& message)
{
	QCorFrame res;
	res.initResponse(req);
	res.setData(JsonProtocolHelper::createJsonResponseError(code, message));
	con.sendResponse(res);
}

void ActionBase::sendDefaultOkResponse(const ActionData& req, const QJsonObject& params)
{
	ActionBase::sendOkResponse(*req.connection.data(), *req.frame.data(), params);
}

void ActionBase::sendDefaultErrorResponse(const ActionData& req, int statusCode, const QString& message)
{
	ActionBase::sendErrorResponse(*req.connection.data(), *req.frame.data(), statusCode, message);
}

void ActionBase::broadcastNotificationToSiblingClients(const ActionData& req, const QString& action, const QJsonObject& params)
{
	QCorFrame f;
	f.setData(JsonProtocolHelper::createJsonRequest(action, params));

	const auto pids = req.server->getSiblingClientIds(req.session->_clientEntity->id);
	foreach (const auto pid, pids)
	{
		const auto sess = req.server->_connections.value(pid);
		if (sess && sess != req.session)
		{
			const auto reply = sess->_connection->sendRequest(f);
			if (reply) QObject::connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
		}
	}
}

void ActionBase::disconnectFromHostDelayed(const ActionData& req)
{
	QMetaObject::invokeMethod(req.connection.data(), "disconnectFromHost", Qt::QueuedConnection);
}

///////////////////////////////////////////////////////////////////////

void AuthenticationAction::run(const ActionData& req)
{
	const auto clientVersion = req.params["version"].toString();
	const auto clientSupportedServerVersions = req.params["supportedversions"].toString();
	const auto username = req.params["username"].toString();
	const auto password = req.params["password"].toString();
	const auto videoEnabled = req.params["videoenabled"].toBool();
	const auto ts3ClientDbId = req.params["ts3_client_database_id"].toVariant().toULongLong();
	const auto peerAddress = req.connection->socket()->peerAddress();

	// Max number of connections (Connection limit).
	if (req.server->_connections.size() > req.server->options().connectionLimit)
	{
		HL_WARN(HL, QString("Server connection limit exceeded. (max=%1)").arg(req.server->options().connectionLimit).toStdString());
		sendDefaultErrorResponse(req, IFVS_STATUS_CONNECTION_LIMIT_REACHED, "Server connection limit exceeded.");
		disconnectFromHostDelayed(req);
		return;
	}

	// Max bandwidth usage (Bandwidth limit).
	if (req.server->_networkUsageMediaSocket.bandwidthRead > req.server->options().bandwidthReadLimit || req.server->_networkUsageMediaSocket.bandwidthWrite > req.server->options().bandwidthWriteLimit)
	{
		HL_WARN(HL, QString("Server bandwidth limit exceeded.").toStdString());
		sendDefaultErrorResponse(req, IFVS_STATUS_BANDWIDTH_LIMIT_REACHED, "Server bandwidth limit exceeded.");
		disconnectFromHostDelayed(req);
		return;
	}

	// Ban check.
	if (req.server->isBanned(peerAddress))
	{
		HL_WARN(HL, QString("Banned user tried to connect. (address=%1)").arg(peerAddress.toString()).toStdString());
		sendDefaultErrorResponse(req, IFVS_STATUS_BANNED, "You are banned from the server.");
		disconnectFromHostDelayed(req);
		return;
	}

	// Compare client version against server version compatibility.
	if (!ELWS::isVersionSupported(clientVersion, IFVS_SOFTWARE_VERSION, clientSupportedServerVersions, IFVS_SERVER_SUPPORTED_CLIENT_VERSIONS))
	{
		HL_WARN(HL, QString("Incompatible version (client=%1; server=%2)").arg(clientVersion).arg(IFVS_SOFTWARE_VERSION).toStdString());
		sendDefaultErrorResponse(req, IFVS_STATUS_INCOMPATIBLE_VERSION, QString("Incompatible version (client=%1; server=%2)").arg(clientVersion).arg(IFVS_SOFTWARE_VERSION));
		disconnectFromHostDelayed(req);
		return;
	}

	// Authenticate.
	if (username.isEmpty() || (!req.server->options().password.isEmpty() && req.server->options().password != password))
	{
		HL_WARN(HL, QString("Authentication failed by user (user=%1)").arg(username).toStdString());
		sendDefaultErrorResponse(req, IFVS_STATUS_UNAUTHORIZED, "Authentication failed by user (empty username or wrong password?)");
		disconnectFromHostDelayed(req);
		return;
	}

	// Ask TS3 Server.
	QtAsync::async(
	    [ = ]()
	{
		const auto& o = req.server->options();
		if (o.ts3Enabled)
		{
			TS3QueryConsoleSocketSync qc;
			if (!qc.start(o.ts3Address, o.ts3Port))
				return false;

			if (!qc.login(o.ts3LoginName, o.ts3LoginPassword))
				return false;

			if (!qc.useByPort(o.ts3VirtualServerPort))
				return false;

			if (!o.ts3Nickname.isEmpty() && !qc.updateNickname(o.ts3Nickname + QString(" %1:%2").arg(qc.localAddress().toString()).arg(qc.localPort())))
				return false;

			// Get client-list and search for client by it's IP address.
			// ATTENTION: It might be possible that there are multiple clients from the same IP address.
			//            So it would be better to check for the "client_database_id" aswell. But older TS3VIDEO versions,
			//            doesn't provide this information.
			auto clientList = qc.clientList();
			auto found = -1;
			if (ts3ClientDbId > 0)
			{
				for (auto i = 0; i < clientList.size(); ++i)
				{
					auto& c = clientList[i];
					auto dbid = c.value("client_database_id").toULongLong();
					auto type = c.value("client_type").toInt();
					auto ip = c.value("connection_client_ip");
					if (dbid == ts3ClientDbId
					        && type == 0
					        && ip.compare(peerAddress.toString()) == 0)
					{
						found = i;
						break;
					}
				}
			}
			else
			{
				// DEPRECATED: The bad way (missing check for database ID)
				//             But required for older client versions (<= 0.3)
				for (auto i = 0; i < clientList.size(); ++i)
				{
					auto& c = clientList[i];
					auto type = c.value("client_type").toInt();
					auto ip = c.value("connection_client_ip");
					if (type == 0
					        && ip.compare(peerAddress.toString()) == 0)
					{
						found = i;
						break;
					}
				}
			}
			if (found < 0)
				return false;

			// Check whether the client is in a valid server group.
			if (!o.ts3AllowedServerGroups.isEmpty())
			{
				auto& client = clientList[found];
				auto clientDbId = client.value("client_database_id").toULongLong();
				if (clientDbId <= 0)
					return false;

				auto sgl = qc.serverGroupsByClientId(clientDbId);
				auto hasGroup = false;
				for (auto i = 0; i < sgl.size() && !hasGroup; ++i)
				{
					auto& sg = sgl[i];
					for (auto k = 0; k < o.ts3AllowedServerGroups.size() && !hasGroup; ++k)
					{
						if (sg.value("sgid").toULongLong() == o.ts3AllowedServerGroups[k])
						{
							hasGroup = true;
							break;
						}
					}
				}
				if (!hasGroup)
				{
					HL_WARN(HL, QString("Client is not a member of a allowed TS3 server group (cldbid=%1)").arg(clientDbId).toStdString());
					return false;
				}
			}

			qc.quit();
		}
		return true;
	},
	[ = ](QVariant data)
	{
		auto b = data.toBool();
		if (!b)
		{
			HL_WARN(HL, QString("Client authorization against TeamSpeak 3 failed (ip=%1)").arg(peerAddress.toString()).toStdString());
			sendDefaultErrorResponse(req, IFVS_STATUS_UNAUTHORIZED, "Authorization against TeamSpeak 3 Server failed. Looks like you do not have the required permission (TS3 Server Group)");
			disconnectFromHostDelayed(req);
			return;
		}

		if (!req.session)
			return;

		// Update self ClientEntity and generate auth-token for media socket.
		req.session->_authenticated = true;
		req.session->_clientEntity->name = username;
		req.session->_clientEntity->videoEnabled = videoEnabled;

		const auto token = QString("%1-%2").arg(req.session->_clientEntity->id).arg(QDateTime::currentDateTimeUtc().toString());
		req.server->_tokens.insert(token, req.session->_clientEntity->id);

		// Respond.
		QJsonObject params;
		params["client"] = req.session->_clientEntity->toQJsonObject();
		params["authtoken"] = token;
		sendDefaultOkResponse(req, params);
	});

}

///////////////////////////////////////////////////////////////////////

void GoodbyeAction::run(const ActionData& req)
{
	sendDefaultOkResponse(req);
	disconnectFromHostDelayed(req);
}

///////////////////////////////////////////////////////////////////////

void HeartbeatAction::run(const ActionData& req)
{
	req.session->_connectionTimeoutTimer.stop();
	req.session->_connectionTimeoutTimer.start(20000);

	sendDefaultOkResponse(req);
}

///////////////////////////////////////////////////////////////////////

void EnableVideoAction::run(const ActionData& req)
{
	req.session->_clientEntity->videoEnabled = true;
	req.server->updateMediaRecipients();

	sendDefaultOkResponse(req);

	// Broadcast to sibling clients.
	QJsonObject params;
	params["client"] = req.session->_clientEntity->toQJsonObject();
	broadcastNotificationToSiblingClients(req, "notify.clientvideoenabled", params);
}

///////////////////////////////////////////////////////////////////////

void DisableVideoAction::run(const ActionData& req)
{
	req.session->_clientEntity->videoEnabled = false;
	req.server->updateMediaRecipients();

	sendDefaultOkResponse(req);

	// Broadcast to sibling clients.
	QJsonObject params;
	params["client"] = req.session->_clientEntity->toQJsonObject();
	broadcastNotificationToSiblingClients(req, "notify.clientvideodisabled", params);
}

///////////////////////////////////////////////////////////////////////

void EnableRemoteVideoAction::run(const ActionData& req)
{
	const auto clientId = req.params["clientid"].toInt();

	auto& set = req.session->_clientEntity->remoteVideoExcludes;
	if (set.remove(clientId))
	{
		req.server->updateMediaRecipients();
	}

	sendDefaultOkResponse(req);
}

///////////////////////////////////////////////////////////////////////

void DisableRemoteVideoAction::run(const ActionData& req)
{
	const auto clientId = req.params["clientid"].toInt();

	auto& set = req.session->_clientEntity->remoteVideoExcludes;
	if (!set.contains(clientId))
	{
		set.insert(clientId);
		req.server->updateMediaRecipients();
	}

	sendDefaultOkResponse(req);
}

///////////////////////////////////////////////////////////////////////

void JoinChannelAction::run(const ActionData& req)
{
	int channelId = 0;
	if (req.action == "joinchannel")
	{
		channelId = req.params["channelid"].toInt();
	}
	else if (req.action == "joinchannelbyidentifier")
	{
		auto ident = req.params["identifier"].toString();
		channelId = qHash(ident);
	}
	const auto password = req.params["password"].toString();

	// Validate parameters.
	if (channelId == 0 || (!req.server->options().validChannels.isEmpty() && !req.server->options().validChannels.contains(channelId)))
	{
		sendDefaultErrorResponse(req, IFVS_STATUS_INVALID_PARAMETERS, QString("Invalid channel id (channelid=%1)").arg(channelId));
		return;
	}

	// Retrieve channel information (It is not guaranteed that the channel already exists).
	auto channelEntity = req.server->_channels.value(channelId);

	// Verify password.
	if (channelEntity && !channelEntity->password.isEmpty() && channelEntity->password.compare(password) != 0)
	{
		sendDefaultErrorResponse(req, IFVS_STATUS_UNAUTHORIZED, QString("Wrong channel password (channelid=%1)").arg(channelId));
		return;
	}

	// Create channel, if it doesn't exists yet.
	if (!channelEntity)
	{
		channelEntity = new ServerChannelEntity();
		channelEntity->id = channelId;
		channelEntity->isPasswordProtected = !password.isEmpty();
		channelEntity->password = password;
		req.server->_channels.insert(channelEntity->id, channelEntity);
	}

	// Associate the client's membership to the channel.
	req.server->addClientToChannel(req.session->_clientEntity->id, channelId);
	req.server->updateMediaRecipients();

	// Build response with information about the channel.
	auto participants = req.server->_participants[channelEntity->id];
	QJsonObject params;
	params["channel"] = channelEntity->toQJsonObject();
	QJsonArray paramsParticipants;
	foreach (auto clientId, participants)
	{
		auto client = req.server->_clients.value(clientId);
		if (client)
		{
			paramsParticipants.append(client->toQJsonObject());
		}
	}
	params["participants"] = paramsParticipants;
	sendDefaultOkResponse(req, params);

	// Notify participants about the new client.
	params = QJsonObject();
	params["channel"] = channelEntity->toQJsonObject();
	params["client"] = req.session->_clientEntity->toQJsonObject();
	broadcastNotificationToSiblingClients(req, "notify.clientjoinedchannel", params);
}

///////////////////////////////////////////////////////////////////////

void LeaveChannelAction::run(const ActionData& req)
{
	const auto channelId = req.params["channelid"].toInt();

	// Find channel.
	auto channelEntity = req.server->_channels.value(channelId);
	if (!channelEntity)
	{
		sendDefaultErrorResponse(req, IFVS_STATUS_INVALID_PARAMETERS, QString("Invalid channel id (channelid=%1)").arg(channelId));
		return;
	}

	sendDefaultOkResponse(req);

	// Notify participants.
	QJsonObject params;
	params["channel"] = channelEntity->toQJsonObject();
	params["client"] = req.session->_clientEntity->toQJsonObject();
	broadcastNotificationToSiblingClients(req, "notify.clientleftchannel", params);

	// Leave channel.
	req.server->removeClientFromChannel(req.session->_clientEntity->id, channelId);
	req.server->updateMediaRecipients();
}

///////////////////////////////////////////////////////////////////////

void AdminAuthAction::run(const ActionData& req)
{
	const auto password = req.params["password"].toString();

	// Verify password.
	const auto adminPassword = req.server->options().adminPassword;
	if (password.isEmpty() || adminPassword.isEmpty() || password.compare(adminPassword) != 0)
	{
		QCorFrame res;
		res.initResponse(*req.frame.data());
		res.setData(JsonProtocolHelper::createJsonResponseError(1, "Wrong password"));
		req.connection->sendResponse(res);
		return;
	}

	req.session->_isAdmin = true;

	sendDefaultOkResponse(req);
}

///////////////////////////////////////////////////////////////////////

void KickClientAction::run(const ActionData& req)
{
	const auto clientId = req.params["clientid"].toInt();
	const auto ban = req.params["ban"].toBool();

	// Find client session.
	auto sess = req.server->_connections.value(clientId);
	if (!sess)
	{
		QCorFrame res;
		res.initResponse(*req.frame.data());
		res.setData(JsonProtocolHelper::createJsonResponseError(1, QString("Unknown client")));
		sess->_connection->sendResponse(res);
		return;
	}

	// Kick client (+notify the client about it).
	QJsonObject params;
	params["client"] = req.session->_clientEntity->toQJsonObject();

	QCorFrame f;
	f.setData(JsonProtocolHelper::createJsonRequest("notify.kicked", params));
	const auto reply = sess->_connection->sendRequest(f);
	QObject::connect(reply, &QCorReply::finished, reply, &QCorReply::deleteLater);
	QMetaObject::invokeMethod(sess->_connection.data(), "disconnectFromHost", Qt::QueuedConnection);

	// Update ban-list.
	if (ban)
	{
		req.server->ban(QHostAddress(req.session->_clientEntity->mediaAddress));
	}

	sendDefaultOkResponse(req);
}

///////////////////////////////////////////////////////////////////////