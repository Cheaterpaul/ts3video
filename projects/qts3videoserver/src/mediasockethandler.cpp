#include "mediasockethandler.h"

#include <QString>
#include <QDataStream>
#include <QTimer>

#include "humblelogging/api.h"

#include "medprotocol.h"

#include "ts3videoserver.h"

HUMBLE_LOGGER(HL, "server.mediasocket");

///////////////////////////////////////////////////////////////////////

MediaSocketHandler::MediaSocketHandler(quint16 port, QObject *parent) :
  QObject(parent),
  _port(port),
  _socket(this),
  _bytesRead(0),
  _bytesWritten(0),
  _transferRateRead(0.0),
  _transferRateWrite(0.0),
  _bandwidthBytesReadTemp(0),
  _bandwidthBytesWrittenTemp(0)
{
  connect(&_socket, &QUdpSocket::readyRead, this, &MediaSocketHandler::onReadyRead);

  // Update bandwidth status every X seconds.
  auto bandwidthTimer = new QTimer(this);
  bandwidthTimer->setInterval(1500);
  bandwidthTimer->start();
  QObject::connect(bandwidthTimer, &QTimer::timeout, [this]() {
    auto elapsedms = _bandwidthCalcTime.elapsed();
    _bandwidthCalcTime.restart();
    // Calculate READ transfer rate.
    if (elapsedms > 0) {
      auto diff = _bytesRead - _bandwidthBytesReadTemp;
      if (diff > 0) {
        _transferRateRead = ((double)diff / elapsedms) * 1000;
      }
      _bandwidthBytesReadTemp = _bytesRead;
    }
    // Calculate WRITE transfer rate.
    if (elapsedms > 0) {
      auto diff = _bytesWritten - _bandwidthBytesWrittenTemp;
      if (diff > 0) {
        _transferRateWrite = ((double)diff / elapsedms) * 1000;
      }
      _bandwidthBytesWrittenTemp = _bytesRead;
    }
    emit transferRatesChanged(_transferRateRead, _transferRateWrite);
  });
}

MediaSocketHandler::~MediaSocketHandler()
{
  _socket.close();
}

bool MediaSocketHandler::init()
{
  if (!_socket.bind(QHostAddress::Any, _port, QAbstractSocket::DontShareAddress)) {
    HL_ERROR(HL, QString("Can not bind to UDP port (port=%1)").arg(_port).toStdString());
    return false;
  }
  return true;
}

void MediaSocketHandler::setRecipients(const MediaRecipients &rec)
{
  _recipients = rec;
}

void MediaSocketHandler::onReadyRead()
{
  while (_socket.hasPendingDatagrams()) {
    // Read datagram.
    QByteArray data;
    QHostAddress senderAddress;
    quint16 senderPort;
    data.resize(_socket.pendingDatagramSize());
    _socket.readDatagram(data.data(), data.size(), &senderAddress, &senderPort);
    _bytesRead += data.size();

    // Check magic.
    QDataStream in(data);
    in.setByteOrder(QDataStream::BigEndian);
    UDP::Datagram dg;
    in >> dg.magic;
    if (dg.magic != UDP::Datagram::MAGIC) {
      HL_WARN(HL, QString("Received invalid datagram (size=%1; data=%2)").arg(data.size()).arg(QString(data)).toStdString());
      continue;
    }

    // Handle by type.
    in >> dg.type;
    switch (dg.type) {

      // Authentication
      case UDP::AuthDatagram::TYPE: {
        UDP::AuthDatagram dgauth;
        in >> dgauth.size;
        dgauth.data = new UDP::dg_byte_t[dgauth.size];
        auto read = in.readRawData((char*)dgauth.data, dgauth.size);
        if (read != dgauth.size) {
          // Error.
          continue;
        }
        auto token = QString::fromUtf8((char*)dgauth.data, dgauth.size);
        emit tokenAuthentication(token, senderAddress, senderPort);
        break;
      }

      // Video data.
      case UDP::VideoFrameDatagram::TYPE: {
        auto senderId = MediaSenderEntity::createID(senderAddress, senderPort);
        const auto &senderEntity = _recipients.id2sender[senderId];
        for (auto i = 0; i < senderEntity.receivers.size(); ++i) {
          const auto &receiverEntity = senderEntity.receivers[i];
          _socket.writeDatagram(data, receiverEntity.address, receiverEntity.port);
          _bytesWritten += data.size();
        }
        break;
      }

      // Video recovery.
      case UDP::VideoFrameRecoveryDatagram::TYPE: {
        HL_TRACE(HL, QString("Process video frame recovery datagram.").toStdString());

        UDP::VideoFrameRecoveryDatagram dgrec;
        in >> dgrec.sender;
        //in >> dgrec.frameId;
        //in >> dgrec.index;

        // Send to specific receiver only.
        const auto &receiver = _recipients.clientid2receiver[dgrec.sender];
        if (receiver.address.isNull() || receiver.port == 0) {
          HL_WARN(HL, QString("Unknown receiver for recovery frame (client-id=%1)").arg(dgrec.sender).toStdString());
          continue;
        }
        _socket.writeDatagram(data, receiver.address, receiver.port);
        _bytesWritten += data.size();
        break;
      }
    }

  } // while (datagrams)
}
