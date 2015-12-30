#include "hintoverlaywidget.h"

#include <QDebug>

#include <QtCore/QEvent>

#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGraphicsDropShadowEffect>

QPointer<HintOverlayWidget> HintOverlayWidget::HINT;

HintOverlayWidget::HintOverlayWidget(QWidget* content, QWidget* target, QWidget* parent) :
	QFrame(parent),
	_content(content),
	_target(target)
{
	setWindowFlags(windowFlags() | Qt::Tool);
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

	auto l = new QBoxLayout(QBoxLayout::TopToBottom);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	setLayout(l);

	l->addWidget(content);

	qApp->installEventFilter(this);
}

HintOverlayWidget::~HintOverlayWidget()
{
	qApp->removeEventFilter(this);
}

QWidget* HintOverlayWidget::showHint(QWidget* content, QWidget* target)
{
	if (HINT)
	{
		hideHint();
	}

	HINT = new HintOverlayWidget(content, target, nullptr);

	auto pos = target->mapToGlobal(target->rect().topRight());
	HINT->move(pos);
	HINT->show();
	return HINT;
}

void HintOverlayWidget::hideHint()
{
	if (!HINT)
	{
		return;
	}
	HINT->deleteLater();
	HINT.clear();
}

bool HintOverlayWidget::eventFilter(QObject* obj, QEvent* e)
{
	switch (e->type())
	{
		case QEvent::MouseMove:
		{
			auto ev = static_cast<QMouseEvent*>(e);
			auto gpos = ev->globalPos();
			qDebug() << gpos;
			if (!_target->rect().contains(_target->mapFromGlobal(gpos)) && !_content->rect().contains(_content->mapFromGlobal(gpos)))
			{
				hideHint();
			}
			break;
		}
	}
	return QFrame::eventFilter(obj, e);
}