#include "conferencevideowindow.h"

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
#include <QBoxLayout>
#include <QStatusBar>
#include <QLabel>
#include <QMenuBar>
#include <QGraphicsDropShadowEffect>

#include <QtMultimedia/QCamera>

#include "humblelogging/api.h"

#include "libqtasync/qtasync.h"

#include "libqtcorprotocol/qcorreply.h"
#include "libqtcorprotocol/qcorframe.h"

#include "videolib/elws.h"
#include "videolib/cliententity.h"
#include "videolib/channelentity.h"
#include "videolib/networkusageentity.h"
#include "videolib/jsonprotocolhelper.h"

#include "networkclient/clientlistmodel.h"
#include "util/qwidgetutil.h"
#include "clientcameravideowidget.h"
#include "remoteclientvideowidget.h"
#include "tileviewwidget.h"
#include "adminauthwidget.h"
#include "aboutwidget.h"

#include "video/conferencevideowindowsidebar.h"
#include "video/videosettingswidget.h"

#if defined(OCS_INCLUDE_AUDIO)
#include <QAudioDeviceInfo>
#include "audio/audioframegrabber.h"
#include "audio/audioframeplayer.h"
#endif

HUMBLE_LOGGER(HL, "client.logic");

///////////////////////////////////////////////////////////////////////
// Local Helpers
///////////////////////////////////////////////////////////////////////

static QSharedPointer<QCamera> createCameraFromOptions(const ConferenceVideoWindow::Options& opts)
{
	QCameraInfo cameraInfo;
	foreach (auto ci, QCameraInfo::availableCameras())
	{
		if (ci.deviceName() == opts.cameraDeviceId)
		{
			cameraInfo = ci;
			break;
		}
	}
	auto cam = cameraInfo.isNull() ? QSharedPointer<QCamera>() : QSharedPointer<QCamera>(new QCamera(cameraInfo));
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
	if (cam)
	{
		auto sett = cam->viewfinderSettings();
		sett.setResolution(opts.cameraResolution);
		cam->setViewfinderSettings(sett);
	}
#endif
	return cam;
}

#if defined(OCS_INCLUDE_AUDIO)
static QAudioFormat createAudioFormat()
{
	QAudioFormat format;
	format.setSampleRate(8000);
	format.setChannelCount(1);
	format.setSampleSize(16);
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::UnSignedInt);
	return format;
}

static QSharedPointer<QAudioInput> createMicrophoneFromOptions(const ConferenceVideoWindow::Options& opts)
{
	auto info = QAudioDeviceInfo::defaultInputDevice();
	foreach (auto item, QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
	{
		if (item.deviceName() == opts.audioInputDeviceId)
		{
			info = item;
			break;
		}
	}
	auto format = createAudioFormat();
	if (!info.isFormatSupported(format))
	{
		return QSharedPointer<QAudioInput>();
	}
	return QSharedPointer<QAudioInput>(new QAudioInput(info, format));
}
#endif

///////////////////////////////////////////////////////////////////////

void ConferenceVideoWindow::addDropShadowEffect(QWidget* widget)
{
	auto dropShadow = new QGraphicsDropShadowEffect(widget);
	dropShadow->setOffset(0, 0);
	dropShadow->setBlurRadius(5);
	dropShadow->setColor(QColor(Qt::black));
	widget->setGraphicsEffect(dropShadow);
}

RemoteClientVideoWidget* ConferenceVideoWindow::createRemoteVideoWidget(const ConferenceVideoWindow::Options& opts, const ClientEntity& client, QWidget* parent)
{
	auto w = new RemoteClientVideoWidget(opts.uiVideoHardwareAccelerationEnabled, parent);
	if (client.id > 0)
	{
		w->setClient(client);
	}
	return w;
}

///////////////////////////////////////////////////////////////////////

ConferenceVideoWindow::ConferenceVideoWindow(const QSharedPointer<NetworkClient>& nc, QWidget* parent, Qt::WindowFlags flags) :
	QMainWindow(parent, flags),
	_networkClient(nc),
	_layout(nullptr),
	_sidebar(nullptr),
	_view(nullptr),
	_statusbar(nullptr)
{
	// Prepare central view (Sidebar + Video view).
	if (true)
	{
		auto w = new QWidget(this);
		_layout = new QBoxLayout(QBoxLayout::LeftToRight);
		_layout->setContentsMargins(0, 0, 0, 0);
		_layout->setSpacing(0);
		w->setLayout(_layout);
		setCentralWidget(w);

		// Sidebar with actions.
		_sidebar = new ConferenceVideoWindowSidebar(this);
		_layout->addWidget(_sidebar, 0);

		// Central view widget.
		_view = new TileViewWidget(this);
		_layout->addWidget(_view, 1);
	}

	setupMenu();
	setupStatusBar();

	// Network related initializations.
	if (true)
	{
		connect(_networkClient.data(), &NetworkClient::error, this, &ConferenceVideoWindow::onError);
		connect(_networkClient.data(), &NetworkClient::serverError, this, &ConferenceVideoWindow::onServerError);
		connect(_networkClient.data(), &NetworkClient::clientJoinedChannel, this, &ConferenceVideoWindow::onClientJoinedChannel);
		connect(_networkClient.data(), &NetworkClient::clientLeftChannel, this, &ConferenceVideoWindow::onClientLeftChannel);
		connect(_networkClient.data(), &NetworkClient::clientDisconnected, this, &ConferenceVideoWindow::onClientDisconnected);
		connect(_networkClient.data(), &NetworkClient::newVideoFrame, this, &ConferenceVideoWindow::onNewVideoFrame);

		// Create initial tiles.
		auto m = _networkClient->clientModel();
		for (auto i = 0; i < m->rowCount(); ++i)
		{
			auto c = m->data(m->index(i), ClientListModel::ClientEntityRole).value<ClientEntity>();
			onClientJoinedChannel(c, ChannelEntity());
		}

#if defined(OCS_INCLUDE_AUDIO)
		// Create QAudioInput (microphone).
		if (!_opts.audioInputDeviceId.isEmpty())
		{
			_audioInput = createMicrophoneFromOptions(_opts);

			auto grabber = new AudioFrameGrabber(_audioInput, this);
			QObject::connect(grabber, &AudioFrameGrabber::newFrame, [this](const PcmFrameRefPtr & f)
			{
				_networkClient->sendAudioFrame(f);
			});
		}

		// Create QAudioOutput (headphones).
		if (true)
		{
			_audioPlayer = QSharedPointer<AudioFramePlayer>(new AudioFramePlayer());
			_audioPlayer->setDeviceInfo(QAudioDeviceInfo::defaultOutputDevice());
			_audioPlayer->setFormat(createAudioFormat());
			QObject::connect(_networkClient.data(), &NetworkClient::newAudioFrame, [this](PcmFrameRefPtr f, ocs::clientid_t senderId)
			{
				_audioPlayer->add(f, senderId);
			});
		}

		// Auto turn ON microphone.
		if (_audioInput && _opts.audioInputAutoEnable)
		{
			TileViewWidget* tvw = nullptr;
			if ((tvw = dynamic_cast<TileViewWidget*>(_view)) != nullptr)
			{
				tvw->setAudioInputEnabled(true);
			}
		}
#endif
	}

	// Set window geometry.
	{
		// Default.
		QWidgetUtil::resizeWidgetPerCent(this, 75.0, 75.0);

		// Restore from last session.
		{
			QSettings settings;
			auto savedGeometry = settings.value("UI/ConferenceVideoWindow-Geometry").toByteArray();
			if (!savedGeometry.isEmpty())
				restoreGeometry(savedGeometry);

			auto savedState = settings.value("UI/ConferenceVideoWindow-State").toByteArray();
			if (!savedState.isEmpty())
				restoreState(savedState);
		}
	}
}

ConferenceVideoWindow::~ConferenceVideoWindow()
{
	if (_networkClient)
	{
		_networkClient->disconnect(this);
	}
	if (_camera)
	{
		_camera->stop();
	}
#if defined(OCS_INCLUDE_AUDIO)
	if (_audioInput)
	{
		_audioInput->stop();
	}
#endif
}

const ConferenceVideoWindow::Options& ConferenceVideoWindow::options() const
{
	return _opts;
}

void ConferenceVideoWindow::applyOptions(const Options& opts)
{
	_opts = opts;
	applyVideoInputOptions(opts);
}

void ConferenceVideoWindow::applyVideoInputOptions(const Options& opts)
{
	// Device
	if (_camera)
	{
		_camera->stop();
		_camera->unload();
		_camera.clear();
	}
	_camera = createCameraFromOptions(opts);
	emit cameraChanged();

	// Auto turn ON camera.
	if (_camera && _opts.cameraAutoEnable)
	{
		_sidebar->setVideoEnabled(true);
	}
	else
	{
		_sidebar->setVideoEnabled(false);
	}
}

void ConferenceVideoWindow::loadOptionsFromConfig(Options& opts)
{
	QSettings s;
	opts.cameraDeviceId = s.value("Video/InputDeviceId", opts.cameraDeviceId).toString();
	opts.cameraResolution = s.value("Video/InputDeviceResolution", opts.cameraResolution).toSize();
	opts.cameraBitrate = s.value("Video/InputDeviceBitrate", opts.cameraBitrate).toInt();
	opts.cameraAutoEnable = s.value("Video/InputDeviceAutoEnable", opts.cameraAutoEnable).toBool();
	opts.uiVideoHardwareAccelerationEnabled = s.value("UI/VideoHardwareAccelerationEnabled", opts.uiVideoHardwareAccelerationEnabled).toBool();
}

void ConferenceVideoWindow::saveOptionsToConfig(const Options& opts)
{
	QSettings s;
	s.setValue("Video/InputDeviceId", opts.cameraDeviceId);
	s.setValue("Video/InputDeviceResolution", opts.cameraResolution);
	s.setValue("Video/InputDeviceBitrate", opts.cameraBitrate);
	s.setValue("Video/InputDeviceAutoEnable", opts.cameraAutoEnable);
	s.setValue("UI/VideoHardwareAccelerationEnabled", opts.uiVideoHardwareAccelerationEnabled);
}

QSharedPointer<NetworkClient> ConferenceVideoWindow::networkClient() const
{
	return _networkClient;
}

QSharedPointer<QCamera> ConferenceVideoWindow::camera() const
{
	return _camera;
}

#if defined(OCS_INCLUDE_AUDIO)
QSharedPointer<QAudioInput> ConferenceVideoWindow::audioInput()
{
	return _audioInput;
}
#endif

void ConferenceVideoWindow::showVideoSettingsDialog()
{
	VideoSettingsDialog w(_networkClient, this);
	w.setModal(true);
	w.preselect(_opts);
	if (w.exec() != QDialog::Accepted)
		return;

	_opts = w.values();
	applyVideoInputOptions(_opts);
}

void ConferenceVideoWindow::setupMenu()
{
	auto menuBar = new QMenuBar(this);
	setMenuBar(menuBar);

	if (true)
	{
		auto confMenu = menuBar->addMenu(QIcon(), tr("Conference"));

		auto adminAuthAction = confMenu->addAction(QIcon(":/ic_lock_grey600_48dp.png"), tr("Login as admin..."));
		QObject::connect(adminAuthAction, &QAction::triggered, this, &ConferenceVideoWindow::onActionLoginAsAdminTriggered);

		confMenu->addSeparator();

		auto aboutAction = confMenu->addAction(QIcon(":/ic_info_outline_grey600_48dp.png"), tr("About"));
		QObject::connect(aboutAction, &QAction::triggered, this, &ConferenceVideoWindow::onActionAboutTriggered);

		confMenu->addSeparator();

		auto exitAction = confMenu->addAction(QIcon(), tr("Exit"));
		QObject::connect(exitAction, &QAction::triggered, this, &ConferenceVideoWindow::onActionExitTriggered);
	}

	if (true)
	{
		auto videoMenu = menuBar->addMenu(QIcon(), tr("Video"));

		auto videoSettingsAction = videoMenu->addAction(QIcon(), tr("Video settings..."));
		QObject::connect(videoSettingsAction, &QAction::triggered, this, &ConferenceVideoWindow::onActionVideoSettingsTriggered);
	}
}

void ConferenceVideoWindow::setupStatusBar()
{
	_statusbar = new QStatusBar(this);
	setStatusBar(_statusbar);

	// Network bandwidth status
	if (true)
	{
		auto bandwidthRead = new QLabel("D: 0.0 KB/s");
		bandwidthRead->setObjectName("bandwidthRead");
		bandwidthRead->setMinimumWidth(bandwidthRead->fontMetrics().averageCharWidth() * 35);
		_statusbar->addPermanentWidget(bandwidthRead);

		auto bandwidthWrite = new QLabel("U: 0.0 KB/s");
		bandwidthWrite->setObjectName("bandwidthWrite");
		bandwidthWrite->setMinimumWidth(bandwidthWrite->fontMetrics().averageCharWidth() * 35);
		_statusbar->addPermanentWidget(bandwidthWrite);

		QObject::connect(_networkClient.data(), &NetworkClient::networkUsageUpdated, [this, bandwidthRead, bandwidthWrite](const NetworkUsageEntity & networkUsage)
		{
			bandwidthRead->setText(QString("D: %1")
								   .arg(ELWS::humanReadableBandwidth(networkUsage.bandwidthRead)));
			bandwidthWrite->setText(QString("U: %1")
									.arg(ELWS::humanReadableBandwidth(networkUsage.bandwidthWrite)));

			const auto sumText = tr("Received: %1\nSent: %2")
								 .arg(ELWS::humanReadableSize(networkUsage.bytesRead))
								 .arg(ELWS::humanReadableSize(networkUsage.bytesWritten));
			bandwidthRead->setToolTip(sumText);
			bandwidthWrite->setToolTip(sumText);
		});
	}
}

void ConferenceVideoWindow::onActionVideoSettingsTriggered()
{
	showVideoSettingsDialog();
}

void ConferenceVideoWindow::onActionLoginAsAdminTriggered()
{
	AdminAuthWidget w(_networkClient, this);
	w.setModal(true);
	w.exec();

	auto action = qobject_cast<QAction*>(sender());
	if (_networkClient->isAdmin() && action)
	{
		action->setEnabled(false);
		action->setText(tr("You are logged in as admin"));
	}
}

void ConferenceVideoWindow::onActionAboutTriggered()
{
	AboutWidget w(this);
	QWidgetUtil::resizeWidgetPerCent(&w, 35, 40);
	QWidgetUtil::centerWidget(&w);
	w.exec();
}

void ConferenceVideoWindow::onActionExitTriggered()
{
	close();
}

void ConferenceVideoWindow::onError(QAbstractSocket::SocketError socketError)
{
	HL_INFO(HL, QString("Socket error (error=%1; message=%2)").arg(socketError).arg(_networkClient->socket()->errorString()).toStdString());
	showError(tr("Network socket error."), _networkClient->socket()->errorString());
}

void ConferenceVideoWindow::onServerError(int code, const QString& message)
{
	HL_INFO(HL, QString("Server error (error=%1; message=%2)").arg(code).arg(message).toStdString());
	showError(tr("Server error."),  QString("%1: %2").arg(code).arg(message));
}

void ConferenceVideoWindow::onClientJoinedChannel(const ClientEntity& client, const ChannelEntity& channel)
{
	HL_INFO(HL, QString("Client joined channel (client-id=%1; channel-id=%2)").arg(client.id).arg(channel.id).toStdString());
	if (client.id != _networkClient->clientEntity().id)
	{
		_view->addClient(client, channel);
	}
}

void ConferenceVideoWindow::onClientLeftChannel(const ClientEntity& client, const ChannelEntity& channel)
{
	HL_INFO(HL, QString("Client left channel (client-id=%1; channel-id=%2)").arg(client.id).arg(channel.id).toStdString());
	_view->removeClient(client, channel);
}

void ConferenceVideoWindow::onClientDisconnected(const ClientEntity& client)
{
	HL_INFO(HL, QString("Client disconnected (client-id=%1)").arg(client.id).toStdString());
	_view->removeClient(client, ChannelEntity());
}

void ConferenceVideoWindow::onNewVideoFrame(YuvFrameRefPtr frame, ocs::clientid_t senderId)
{
	_view->updateClientVideo(frame, senderId);
}

void ConferenceVideoWindow::onReplyFinsihedHandleError()
{
	auto reply = qobject_cast<QCorReply*>(sender());

	int status = 0;
	QString error;
	QJsonObject params;
	if (!JsonProtocolHelper::fromJsonResponse(reply->frame()->data(), status, params, error))
	{
		QMessageBox::critical(this,
							  tr("Network protocol error"),
							  QString("Can not parse response:\n%1").arg(QString(reply->frame()->data())));
		return;
	}
	else if (status != 0)
	{
		QMessageBox::warning(this,
							 tr("Error from server"),
							 QString("%1: %2").arg(status).arg(error));
		return;
	}
}

void ConferenceVideoWindow::closeEvent(QCloseEvent* e)
{
	QSettings settings;
	settings.setValue("UI/ConferenceVideoWindow-Geometry", saveGeometry());
	settings.setValue("UI/ConferenceVideoWindow-State", saveState());

	auto reply = networkClient()->goodbye();
	QCORREPLY_AUTODELETE(reply);
}

void ConferenceVideoWindow::showResponseError(int status, const QString& errorMessage, const QString& details)
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

void ConferenceVideoWindow::showError(const QString& shortText, const QString& longText)
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