#ifndef TILEVIEWWIDGETPRIVATE
#define TILEVIEWWIDGETPRIVATE

#include "tileviewwidget.h"

#include <QSize>
#include <QHash>
#include <QObject>
#include <QFrame>

class QPushButton;
class QLabel;
class QCamera;
class FlowLayout;
class RemoteClientVideoWidget;
class TileViewTileWidget;
class TileViewCameraWidget;
class TileViewUserListWidget;

///////////////////////////////////////////////////////////////////////

class TileViewWidgetPrivate : public QObject
{
	Q_OBJECT

public:
	TileViewWidgetPrivate(TileViewWidget* o) :
		QObject(o),
		owner(o),
		tilesAspectRatio(4, 3),
		tilesCurrentSize(tilesAspectRatio),
		tilesLayout(nullptr),
		cameraWidget(nullptr),
		zoomInButton(nullptr),
		zoomOutButton(nullptr)
	{}

public:
	TileViewWidget* owner;

	QSize tilesAspectRatio;
	QSize tilesCurrentSize;

	FlowLayout* tilesLayout;
	FlowLayout* noVideoTilesLayout;
	TileViewCameraWidget* cameraWidget;
	QPushButton* zoomInButton;
	QPushButton* zoomOutButton;

	QSharedPointer<QCamera> camera;
	QHash<int, TileViewTileWidget*> tilesMap; ///< Maps client's ID to it's widget.
};

///////////////////////////////////////////////////////////////////////

class TileViewCameraWidget : public QFrame
{
	Q_OBJECT
	friend class TileViewWidget;

public:
	TileViewCameraWidget(TileViewWidget* tileView, QWidget* parent = nullptr);

private slots:
	void onCameraChanged();

private:
	TileViewWidget* _tileView;
	class QBoxLayout* _mainLayout;
	ClientCameraVideoWidget* _cameraWidget;
};

///////////////////////////////////////////////////////////////////////

class TileViewTileWidget : public QFrame
{
	Q_OBJECT
	friend class TileViewWidget;

public:
	TileViewTileWidget(const ClientEntity& client, QWidget* parent = nullptr);

private:
	RemoteClientVideoWidget* _videoWidget;
};

#endif