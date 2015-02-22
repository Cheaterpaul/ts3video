#ifdef _WIN32
#include <cstdio>
#include <Windows.h>
#define GetCurrentWorking
#endif

#ifdef __linux__
#include <cstdio>
#endif

#include "QtCore/QString"
#include "QtCore/QList"
#include "QtCore/QProcess"
#include "QtCore/QFile"
#include "QtCore/QDir"

#include "public_errors.h"
#include "public_errors_rare.h"
#include "public_definitions.h"
#include "public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin.h"
/*
static QFile *_log_file = 0;

void initializePlugin()
{
  // Initialize logging.
  QString log_file_path = QDir::tempPath() + "/sw_ts3_plugin.log";
  if (!_log_file) {
    _log_file = new QFile(log_file_path);
    if (!_log_file->open(QIODevice::WriteOnly | QIODevice::Append)) {
      fprintf(stderr, "Can not open log file: %s\n",
          _log_file->errorString().toStdString().c_str());
      delete _log_file;
      _log_file = 0;
    }
  }
}

void shutdownPlugin()
{
  // Close log file.
  if (_log_file && _log_file->isOpen()) {
    _log_file->flush();
    _log_file->close();
    delete _log_file;
    _log_file = 0;
  }
}

void startHangout(const char *ts_ip, uint64 ts_port, const char *ts_channel_name, const char *ts_username)
{
  // Build unique channel name.
  QString sw_channel_name = QString::fromUtf8(ts_ip) + ":" + QString::number(ts_port) + ":" + QString::fromUtf8(ts_channel_name);
  QString sw_user_name = QString::fromUtf8(ts_username);
  sw_log(QString("Start new wall (channel_name=%1; username=%2)").arg(sw_channel_name).arg(sw_user_name).toStdString().c_str());

  // Search client.exe
  QString proc_file_path;
  QStringList proc_parameters;
  proc_parameters << "--auto-enable-webcam";
  proc_parameters << "--auto-enable-webcam-stream";
  proc_parameters << QString("--auto-connect %1").arg("insanefactory.com");
  //proc_parameters << QString("--auto-connect %1").arg(QString::fromUtf8(ts_ip));
  proc_parameters << QString("--auto-join-channel-name \"%1\"").arg(sw_channel_name);
  proc_parameters << QString("--auto-join-channel-force").arg(sw_channel_name);
  proc_parameters << QString("--nickname \"%1\"").arg(sw_user_name);

#ifdef _WIN32
  char file_path[2048];
  if (GetModuleFileName(NULL, file_path, 2048) <= 0) {
    sw_log("Can not find module file path.");
    return;
  }
  sw_log(QString("Current module file name (path=%1)").arg(file_path).toStdString().c_str());
  proc_file_path = QFileInfo(file_path).path() + "/plugins/sw_ts3_plugin/client.exe";
#endif

#ifdef __linux__
  proc_file_path = "/opt/streamwall/streamwall.exe";
#endif

  sw_log(QString("Start process (path=%1; args=%2)").arg(proc_file_path).arg(proc_parameters.join(" ")).toStdString().c_str());

#ifdef _WIN32
  std::string lpFile = proc_file_path.toLocal8Bit().constData();
  std::string lpParameters = proc_parameters.join(" ").toLocal8Bit().constData();

  SHELLEXECUTEINFO exec_info;
  memset(&exec_info, 0, sizeof(exec_info));
  exec_info.cbSize = sizeof(exec_info);
  exec_info.hwnd = NULL;
  exec_info.lpVerb = "open";
  exec_info.lpFile = lpFile.c_str();
  exec_info.lpParameters = lpParameters.c_str();
  exec_info.lpDirectory = NULL;
  exec_info.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteEx(&exec_info)) {
    sw_log(QString("Can not execute process (file=%1; args=%2)")
        .arg(proc_file_path).arg(proc_parameters.join(" ")).toStdString().c_str());
  }
#endif
}

void sw_log(const char *message)
{
  if (!_log_file)
    return;
  _log_file->write(message);
  _log_file->write("\r\n");
  _log_file->flush();
}
*/
///////////////////////////////////////////////////////////////////////////////
// Trash?
///////////////////////////////////////////////////////////////////////////////

//ShellExecute(NULL, L"open", proc_file_path.toStdWString().c_str(), proc_parameters.toStdWString().c_str(), NULL, SW_SHOWNORMAL);
