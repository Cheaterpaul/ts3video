#pragma once

#include <memory>

#include <QString>
#include <QList>
#include <QDateTime>
#include <QStringList>
#include <QFuture>

namespace lic {

class License
{
public:
	QString version;
	QString id;
	QDateTime createdOn;

	QDateTime expiresOn;
	bool expiresOnNever = false;

	QList<QString> validVersions;

	QString requiredHostId;
	QString requiredMacAddress;
	QString requiredSerial;

	QString customerId;
	QString customerName;
	QString customerEmail;
};


class LicenseXmlReader
{
public:
	std::shared_ptr<License> loadFromFile(const QString& filePath) const;
};


class LicenseManager
{
public:
	static bool isValid(const License& lic);
	static QFuture<std::shared_ptr<License> > loadLicenseFromServer();
};

}