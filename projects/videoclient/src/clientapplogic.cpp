#include "clientapplogic_p.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QMessageBox>
#include <QProgressDialog>
#include <QCameraInfo>
#include <QHostInfo>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QWeakPointer>
#include <QHostAddress>

#include "humblelogging/api.h"

#include "qtasync.h"

#include "qcorreply.h"
#include "qcorframe.h"

#include "elws.h"
#include "cliententity.h"
#include "channelentity.h"
#include "networkusageentity.h"
#include "jsonprotocolhelper.h"

#include "networkclient/clientlistmodel.h"
#include "clientcameravideowidget.h"
#include "remoteclientvideowidget.h"
#include "tileviewwidget.h"

HUMBLE_LOGGER(HL, "client.logic");

///////////////////////////////////////////////////////////////////////

static ClientAppLogic* gFirstInstance;

ClientAppLogic* ClientAppLogic::instance()
{
	return gFirstInstance;
}

///////////////////////////////////////////////////////////////////////

ClientAppLogic::ClientAppLogic(const Options& opts, const QSharedPointer<NetworkClient>& nc, QWidget* parent, Qt::WindowFlags flags) :
	QMainWindow(parent, flags),
	d(new ClientAppLogicPrivate(this))
{
	if (!gFirstInstance)
		gFirstInstance = this;

	d->opts = opts;
	d->nc = nc;

	// Central view widget.
	auto viewWidget = new TileViewWidget(this);
	d->view = viewWidget;
	setCentralWidget(viewWidget);

	// Start camera.
	if (!d->opts.cameraDeviceId.isEmpty())
	{
		d->camera = d->createCameraFromOptions();
		viewWidget->setCamera(d->camera);
	}
}

ClientAppLogic::~ClientAppLogic()
{
	if (gFirstInstance == this)
	{
		gFirstInstance = nullptr;
	}
	if (d->nc)
	{
		d->nc->disconnect(this);
	}
	if (d->view)
	{
		delete d->view;
	}
	if (d->camera)
	{
		d->camera->stop();
	}
}

void ClientAppLogic::start()
{
	connect(d->nc.data(), &NetworkClient::error, this, &ClientAppLogic::onError);
	connect(d->nc.data(), &NetworkClient::serverError, this, &ClientAppLogic::onServerError);
	connect(d->nc.data(), &NetworkClient::clientJoinedChannel, this, &ClientAppLogic::onClientJoinedChannel);
	connect(d->nc.data(), &NetworkClient::clientLeftChannel, this, &ClientAppLogic::onClientLeftChannel);
	connect(d->nc.data(), &NetworkClient::clientDisconnected, this, &ClientAppLogic::onClientDisconnected);
	connect(d->nc.data(), &NetworkClient::newVideoFrame, this, &ClientAppLogic::onNewVideoFrame);

	auto m = d->nc->clientModel();
	for (auto i = 0; i < m->rowCount(); ++i)
	{
		auto c = m->data(m->index(i), ClientListModel::ClientEntityRole).value<ClientEntity>();
		onClientJoinedChannel(c, ChannelEntity());
	}

	// Auto turn ON camera.
	if (d->camera)
	{
		TileViewWidget* tvw = nullptr;
		if ((tvw = dynamic_cast<TileViewWidget*>(d->view)) != nullptr)
			tvw->setVideoEnabled(true);
	}
}

QSharedPointer<NetworkClient> ClientAppLogic::networkClient()
{
	return d->nc;
}

void ClientAppLogic::onError(QAbstractSocket::SocketError socketError)
{
	HL_INFO(HL, QString("Socket error (error=%1; message=%2)").arg(socketError).arg(d->nc->socket()->errorString()).toStdString());
	showError(tr("Network socket error."), d->nc->socket()->errorString());
}

void ClientAppLogic::onServerError(int code, const QString& message)
{
	HL_INFO(HL, QString("Server error (error=%1; message=%2)").arg(code).arg(message).toStdString());
	showError(tr("Server error."),  QString("%1: %2").arg(code).arg(message));
}

void ClientAppLogic::onClientJoinedChannel(const ClientEntity& client, const ChannelEntity& channel)
{
	HL_INFO(HL, QString("Client joined channel (client-id=%1; channel-id=%2)").arg(client.id).arg(channel.id).toStdString());
	if (client.id != d->nc->clientEntity().id)
	{
		d->view->addClient(client, channel);
	}
}

void ClientAppLogic::onClientLeftChannel(const ClientEntity& client, const ChannelEntity& channel)
{
	HL_INFO(HL, QString("Client left channel (client-id=%1; channel-id=%2)").arg(client.id).arg(channel.id).toStdString());
	d->view->removeClient(client, channel);
}

void ClientAppLogic::onClientDisconnected(const ClientEntity& client)
{
	HL_INFO(HL, QString("Client disconnected (client-id=%1)").arg(client.id).toStdString());
	d->view->removeClient(client, ChannelEntity());
}

void ClientAppLogic::onNewVideoFrame(YuvFrameRefPtr frame, int senderId)
{
	d->view->updateClientVideo(frame, senderId);
}

void ClientAppLogic::showEvent(QShowEvent* e)
{
	// TODO: Do not load when it comes back from minimized state.
	QSettings settings;
	restoreGeometry(settings.value("UI/ClientApp-Geometry").toByteArray());
}

void ClientAppLogic::closeEvent(QCloseEvent* e)
{
	QSettings settings;
	settings.setValue("UI/ClientApp-Geometry", saveGeometry());

	auto reply = networkClient()->goodbye();
	QCORREPLY_AUTODELETE(reply);
}

void ClientAppLogic::showResponseError(int status, const QString& errorMessage, const QString& details)
{
	HL_ERROR(HL, QString("Network response error (status=%1; message=%2)").arg(status).arg(errorMessage).toStdString());
	QMessageBox box(this);
	box.setWindowTitle(tr("Warning"));
	box.setIcon(QMessageBox::Warning);
	box.addButton(QMessageBox::Ok);
	box.setText(QString::number(status) + QString(": ") + errorMessage);
	if (!details.isEmpty())
		box.setDetailedText(details);
	box.setMinimumWidth(400);
	box.exec();
}

void ClientAppLogic::showError(const QString& shortText, const QString& longText)
{
	HL_ERROR(HL, QString("%1: %2").arg(shortText).arg(longText).toStdString());
	QMessageBox box(this);
	box.setWindowTitle(tr("Warning"));
	box.setIcon(QMessageBox::Warning);
	box.addButton(QMessageBox::Ok);
	box.setText(shortText);
	box.setDetailedText(longText);
	box.setMinimumWidth(400);
	box.exec();
}

///////////////////////////////////////////////////////////////////////
// Private Impl
///////////////////////////////////////////////////////////////////////

ClientAppLogicPrivate::ClientAppLogicPrivate(ClientAppLogic* o) :
	QObject(o), owner(o), opts(), view(nullptr), cameraWidget(nullptr)
{
}

ClientAppLogicPrivate::~ClientAppLogicPrivate()
{
}

QSharedPointer<QCamera> ClientAppLogicPrivate::createCameraFromOptions() const
{
	auto d = this;
	auto cameraInfo = QCameraInfo::defaultCamera();
	foreach (auto ci, QCameraInfo::availableCameras())
	{
		if (ci.deviceName() == d->opts.cameraDeviceId)
		{
			cameraInfo = ci;
			break;
		}
	}
	return QSharedPointer<QCamera>(new QCamera(cameraInfo));
}