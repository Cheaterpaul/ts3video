#ifndef VIDEOCOLLECTIONWIDGET_H
#define VIDEOCOLLECTIONWIDGET_H

#include <QWidget>

class VideoCollectionWidget : public QWidget
{
  Q_OBJECT
public:
  explicit VideoCollectionWidget(QWidget *parent = 0);
  ~VideoCollectionWidget();
  void addWidget(QWidget *widget);
  void removeWidget(QWidget *widget);
  void setWidgets(const QList<QWidget*> widgets);

private:
  void doGridLayout();

private:
  QList<QWidget*> _widgets;
  int _columnCount;
};

#endif // VIDEOCOLLECTIONWIDGET_H
