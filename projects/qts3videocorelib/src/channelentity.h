#ifndef CHANNELENTITY_H
#define CHANNELENTITY_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

class ChannelEntity
{
public:
  ChannelEntity()
  {}

  ChannelEntity(const ChannelEntity &other)
  {
    this->id = other.id;
    this->name = other.id;
  }
  
  ~ChannelEntity()
  {
  }

  void fromQJsonObject(const QJsonObject &obj)
  {
    id = obj["id"].toInt();
    name = obj["name"].toString();
  }

  QJsonObject toQJsonObject() const
  {
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    return obj;
  }

public:
  int id;
  QString name;
};
Q_DECLARE_METATYPE(ChannelEntity);
#endif