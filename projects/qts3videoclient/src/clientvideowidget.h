#ifndef CLIENTVIDEOWIDGET_H
#define CLIENTVIDEOWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QString>

namespace Ui {
class ClientVideoWidget;
}

class ClientVideoWidget : public QWidget
{
  Q_OBJECT

public:
  explicit ClientVideoWidget(QWidget *parent = 0);
  ~ClientVideoWidget();

public slots:
  void setFrame(const QPixmap &pm);
  void setImage(const QImage &image);
  void setAvatar(const QPixmap &pm);
  void setText(const QString &text);

protected:
  virtual void paintEvent(QPaintEvent *ev);

private:
  Ui::ClientVideoWidget *ui;
  QPixmap _frame;
  QPixmap _avatar;
  QString _text;
};

#endif // CLIENTVIDEOWIDGET_H
