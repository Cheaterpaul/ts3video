#ifndef MEDIASOCKET_H
#define MEDIASOCKET_H

#include <QScopedPointer>
#include <QUdpSocket>
#include <QTimer>

#include "yuvframe.h"
#include "pcmframe.h"

class NetworkUsageEntity;

class MediaSocketPrivate;
class MediaSocket : public QUdpSocket
{
	Q_OBJECT
	friend class MediaSocketPrivate;
	QScopedPointer<MediaSocketPrivate> d;

public:
	MediaSocket(const QString& token, QObject* parent);
	virtual ~MediaSocket();

	bool isAuthenticated() const;
	void setAuthenticated(bool yesno);

	void initVideoEncoder(int width, int height, int bitrate, int fps);
	void resetVideoEncoder();
	void sendVideoFrame(const QImage& image, int senderId);
	
	void resetVideoDecoderOfClient(int senderId);

#if defined(OCS_INCLUDE_AUDIO)
	void sendAudioFrame(const PcmFrameRefPtr& f, int senderId);
#endif

signals:
	/*! Emits with every new arrived and decoded video frame.
	*/
	void newVideoFrame(YuvFrameRefPtr frame, int senderId);

#if defined(OCS_INCLUDE_AUDIO)
	/*!	Emits with every new arrived and decoded audio frame.
	*/
	void newAudioFrame(PcmFrameRefPtr frame, int senderId);
#endif

	/*! Emits periodically with newest calculated network-usage information.
	*/
	void networkUsageUpdated(const NetworkUsageEntity& networkUsage);

protected:
	void sendKeepAliveDatagram();
	void sendAuthTokenDatagram(const QString& token);
	void sendVideoFrame(const QByteArray& frame, quint64 frameId, quint32 senderId);
	void sendVideoFrameRecoveryDatagram(quint64 frameId, quint32 fromSenderId);

#if defined(OCS_INCLUDE_AUDIO)
	void sendAudioFrame(const QByteArray& frame, quint64 frameId, quint32 senderId);
#endif

	virtual void timerEvent(QTimerEvent* ev);

private slots:
	void onSocketStateChanged(QAbstractSocket::SocketState state);
	void onSocketError(QAbstractSocket::SocketError error);
	void onReadyRead();

	void onVideoFrameEncoded(QByteArray frame, int senderId);
	void onVideoFrameDecoded(YuvFrameRefPtr frame, int senderId);

private:
	QTimer _bandwidthTimer;
};

#endif