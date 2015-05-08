#ifndef STARTUPWIDGET_H
#define STARTUPWIDGET_H

#include <QScopedPointer>
#include <QString>
#include <QDialog>
#include "clientapplogic.h"

/*!
  The dialog is used to modify the ClientAppLogic's startup options.
  The field for the channel-identifier is only visible, if no channel-id and channel-identifier is given.
*/
class StartupDialogPrivate;
class StartupDialog : public QDialog
{
  Q_OBJECT
  friend class StartupDialogPrivate;
  QScopedPointer<StartupDialogPrivate> d;

public:
  StartupDialog(QWidget *parent);
  virtual ~StartupDialog();

  ClientAppLogic::Options values() const;
  void setValues(const ClientAppLogic::Options &values);

protected:
  void showEvent(QShowEvent *ev);

private slots:
  void onAccept();
  void validate();
};

#endif