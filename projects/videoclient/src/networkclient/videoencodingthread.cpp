#include "videoencodingthread.h"
#include <QTime>
#include "humblelogging/api.h"
#include "ts3video.h"
#include "vp8frame.h"
#include "vp8encoder.h"

HUMBLE_LOGGER(HL, "networkclient.videoencodingthread");

///////////////////////////////////////////////////////////////////////

#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QAtomicInt>
#include <QPair>
#include <QImage>

class VideoEncodingThreadPrivate
{
public:
	VideoEncodingThreadPrivate(VideoEncodingThread* o) :
		owner(o),
		stopFlag(0),
		recoveryFlag(VP8Frame::NORMAL)
	{}

public:
	VideoEncodingThread* owner;
	QMutex m;
	QWaitCondition queueCond;
	QQueue<QPair<QImage, int> > queue; ///< Replace with RingQueue (Might not keep more than X frames! Otherwise we might get a memory problem.)
	QAtomicInt stopFlag;
	QAtomicInt recoveryFlag;
};

///////////////////////////////////////////////////////////////////////

VideoEncodingThread::VideoEncodingThread(QObject* parent) :
	QThread(parent),
	d(new VideoEncodingThreadPrivate(this))
{
}

VideoEncodingThread::~VideoEncodingThread()
{
	stop();
	wait();
}

void VideoEncodingThread::stop()
{
	d->stopFlag = 1;
	d->queueCond.wakeAll();
}

void VideoEncodingThread::enqueue(const QImage& image, int senderId)
{
	QMutexLocker l(&d->m);
	d->queue.enqueue(qMakePair(image, senderId));
	while (d->queue.size() > 5)
		auto item = d->queue.dequeue();
	d->queueCond.wakeAll();
}

void VideoEncodingThread::enqueueRecovery()
{
	d->recoveryFlag = VP8Frame::KEY;
	d->queueCond.wakeAll();
}

void VideoEncodingThread::run()
{
	const int fps = 24;
	const int fpsTimeMs = 1000 / fps;
	const int bitRate = 100;

	QHash<int, VP8Encoder*> encoders;
	QTime fpsTimer;
	fpsTimer.start();

	d->stopFlag = 0;
	while (d->stopFlag == 0)
	{
		// Get next frame from queue
		QMutexLocker l(&d->m);
		if (d->queue.isEmpty())
		{
			d->queueCond.wait(&d->m);
			continue;
		}
		auto item = d->queue.dequeue();
		l.unlock();

		if (item.first.isNull())
			continue;


		// FPS check
		if (fps > 0 && fpsTimer.elapsed() < fpsTimeMs)
			continue;
		fpsTimer.restart();


		// Convert to YuvFrame.
		auto yuv = YuvFrame::fromQImage(item.first);

		// Get/create encoder
		auto create = false;
		auto encoder = encoders.value(item.second);
		if (!encoder)
			create = true;
		else if (!encoder->isValidFrame(*yuv))
			create = true;

		// Re-/create encoder
		if (create)
		{
			if (encoder)
			{
				encoders.remove(item.second);
				delete encoder;
				encoder = nullptr;
			}

			HL_DEBUG(HL, QString("Create new video encoder (id=%1)").arg(item.second).toStdString());
			encoder = new VP8Encoder();
			if (!encoder->initialize(yuv->width, yuv->height, bitRate, fps))
			{
				HL_ERROR(HL, QString("Can not initialize VP8 video encoder").toStdString());
				d->stopFlag = 1;
				continue;
			}
			encoders.insert(item.second, encoder);
		}

		if (d->recoveryFlag != VP8Frame::NORMAL)
		{
			encoder->setRequestRecoveryFlag(VP8Frame::KEY);
			d->recoveryFlag = VP8Frame::NORMAL;
		}

		// Encode frame
		auto vp8 = encoder->encode(*yuv);
		delete yuv;

		// Serialize VP8Frame.
		QByteArray data;
		QDataStream out(&data, QIODevice::WriteOnly);
		out << *vp8;
		delete vp8;
		emit encoded(data, item.second);

		// DEV Provides plain QImage.
		//if (true) {
		//  static quint64 __frameTime = 1;
		//  VP8Frame vpframe;
		//  vpframe.time = __frameTime++;
		//  vpframe.type = VP8Frame::KEY;
		//  QDataStream out(&vpframe.data, QIODevice::WriteOnly);
		//  out << item.first;

		//  QByteArray data;
		//  QDataStream out2(&data, QIODevice::WriteOnly);
		//  out2 << vpframe;

		//  emit encoded(data, item.second);
		//}

	}

	// Clean up.
	qDeleteAll(encoders);
	encoders.clear();
}