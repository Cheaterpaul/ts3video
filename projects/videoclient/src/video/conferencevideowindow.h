#ifndef CONFERENCEVIDEOWINDOW_H
#define CONFERENCEVIDEOWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QString>
#include <QHash>
#include <QVariant>

#include "libbase/defines.h"

#include "videolib/ts3video.h"
#include "videolib/yuvframe.h"

#include "networkclient/networkclient.h"

#if defined(OCS_INCLUDE_AUDIO)
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioFormat>
#endif

class QWidget;
class QProgressDialog;
class QStatusBar;
class QCamera;
class QBoxLayout;
class ConferenceVideoWindowSidebar;
class RemoteClientVideoWidget;
class TileViewWidget;

class ConferenceVideoWindow : public QMainWindow
{
	Q_OBJECT

public:
	class Options
	{
	public:
		// Video-input (camera) settings
		QString cameraDeviceId = QString();
		QSize cameraResolution = QSize(640, 480);
		int cameraBitrate = 100;
		bool cameraAutoEnable = false;

#if defined(OCS_INCLUDE_AUDIO)
		// The microphones device ID (audio-in).
		QString audioInputDeviceId = QString();
		bool audioInputAutoEnable = false;

		// The headphones device ID (audio-out).
		QString audioOutputDeviceId = QString();
		bool audioOutputAutoEnable = false;
#endif

		// UI settings
		bool uiVideoHardwareAccelerationEnabled = true;
	};

public:
	ConferenceVideoWindow(const Options& options, const QSharedPointer<NetworkClient>& nc, QWidget* parent, Qt::WindowFlags flags);
	virtual ~ConferenceVideoWindow();

	const ConferenceVideoWindow::Options& options() const;
	void applyOptions(const Options& opts);
	void applyVideoInputOptions(const Options& opts);
	static void loadOptionsFromConfig(Options& opts);
	static void saveOptionsToConfig(const Options& opts);

	QSharedPointer<NetworkClient> networkClient() const;
	QSharedPointer<QCamera> camera() const;

#if defined(OCS_INCLUDE_AUDIO)
	QSharedPointer<QAudioInput> audioInput();
#endif

	static void addDropShadowEffect(QWidget* widget);
	static RemoteClientVideoWidget* createRemoteVideoWidget(const ConferenceVideoWindow::Options& opts, const ClientEntity& client, QWidget* parent = nullptr);

public slots:
	void showVideoSettingsDialog();

private:
	void setupMenu();
	void setupStatusBar();

private slots:
	// Gui callbacks
	void onActionVideoSettingsTriggered();
	void onActionLoginAsAdminTriggered();
	void onActionAboutTriggered();
	void onActionExitTriggered();

	// Network callbacks
	void onError(QAbstractSocket::SocketError socketError);
	void onServerError(int code, const QString& message);
	void onClientJoinedChannel(const ClientEntity& client, const ChannelEntity& channel);
	void onClientLeftChannel(const ClientEntity& client, const ChannelEntity& channel);
	void onClientDisconnected(const ClientEntity& client);
	void onNewVideoFrame(YuvFrameRefPtr frame, ocs::clientid_t senderId);

public slots:
	// Network helper
	void onReplyFinsihedHandleError();

protected:
	virtual void closeEvent(QCloseEvent* e);
	void showResponseError(int status, const QString& errorMessage, const QString& details = QString());
	void showError(const QString& shortText, const QString& longText = QString());

signals:
	void cameraChanged();

private:
	ConferenceVideoWindow::Options _opts;                ///< Basic options for video conferencing.
	QSharedPointer<NetworkClient> _networkClient;        ///< Client for network communication.

	// Video stuff.
	QSharedPointer<QCamera> _camera;                     ///< The used camera for this conference.

#if defined(OCS_INCLUDE_AUDIO)
	// Audio stuff.
	QSharedPointer<QAudioInput> _audioInput;
	QSharedPointer<QAudioOutput> _audioOutput;
	QSharedPointer<AudioFramePlayer> _audioPlayer;
#endif

	// GUI stuff.
	QBoxLayout* _layout;
	ConferenceVideoWindowSidebar* _sidebar;              ///< Sidebar with controls.
	TileViewWidget* _view;                               ///< Central view to display all video streams.
	QStatusBar* _statusbar;
};

#endif