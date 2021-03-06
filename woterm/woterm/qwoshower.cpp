#include "qwoshower.h"
#include "qwotermwidgetimpl.h"
#include "qwoshellwidgetimpl.h"
#include "qwoscriptwidgetimpl.h"
#include "qwosessionproperty.h"
#include "qwomainwindow.h"
#include "qwosshconf.h"
#include "qwoevent.h"

#include <QTabBar>
#include <QResizeEvent>
#include <QMessageBox>
#include <QtGlobal>
#include <QSplitter>
#include <QDebug>
#include <QPainter>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QProcess>


QWoShower::QWoShower(QTabBar *tab, QWidget *parent)
    : QStackedWidget (parent)
    , m_tab(tab)
{
    QObject::connect(tab, SIGNAL(tabCloseRequested(int)), this, SLOT(onTabCloseRequested(int)));
    QObject::connect(tab, SIGNAL(currentChanged(int)), this, SLOT(onTabCurrentChanged(int)));
    QObject::connect(tab, SIGNAL(tabBarDoubleClicked(int)), this, SLOT(onTabbarDoubleClicked(int)));
    tab->installEventFilter(this);
}

QWoShower::~QWoShower()
{

}

bool QWoShower::openLocalShell()
{
//    QWoShowerWidget *impl = new QWoShellWidgetImpl(this);
//    impl->setProperty(TAB_TYPE_NAME, ETShell);
//    createTab(impl, "local");
    return true;
}

bool QWoShower::openScriptRuner(const QString& script)
{
    QWoScriptWidgetImpl *impl = new QWoScriptWidgetImpl(this);
    impl->setProperty(TAB_TYPE_NAME, ETScript);
    createTab(impl, script);
    return true;
}

bool QWoShower::openConnection(const QString &target)
{
    QWoShowerWidget *impl = new QWoTermWidgetImpl(target, m_tab,  this);
    impl->setProperty(TAB_TYPE_NAME, ETSsh);
    createTab(impl, target);
    return true;
}

bool QWoShower::openConnection(const QStringList &targets)
{
    if(targets.isEmpty()) {
        return false;
    }
    QStringList mytargets = targets;
    QString target = mytargets.takeFirst();
    QWoTermWidgetImpl *impl = new QWoTermWidgetImpl(target, m_tab,  this);
    impl->setProperty(TAB_TYPE_NAME, ETSsh);
    createTab(impl, target);
    int row = targets.length() > 4 ? 3 : 2;
    for(int r = 1; r < row; r++) {
        impl->joinToVertical(mytargets.takeFirst());
    }
    for(int r = 0; r < row && mytargets.length() > 0; r++) {
        impl->joinToHorizontal(r, mytargets.takeFirst());
    }
    return true;
}

void QWoShower::setBackgroundColor(const QColor &clr)
{
    QPalette pal;
    pal.setColor(QPalette::Background, clr);
    pal.setColor(QPalette::Window, clr);
    setPalette(pal);
}

void QWoShower::openFindDialog()
{
    int idx = m_tab->currentIndex();
    if (idx < 0 || idx > m_tab->count()) {
        return;
    }
    QVariant v = m_tab->tabData(idx);
    QWoShowerWidget *target = v.value<QWoShowerWidget*>();
//    QSplitter *take = m_terms.at(idx);
//    Q_ASSERT(target == take);
    //    take->toggleShowSearchBar();
}

int QWoShower::tabCount()
{
    return m_tab->count();
}

void QWoShower::resizeEvent(QResizeEvent *event)
{
    QSize newSize = event->size();
    QRect rt(0, 0, newSize.width(), newSize.height());
}

void QWoShower::syncGeometry(QWidget *widget)
{
    QRect rt = geometry();
    rt.moveTo(0, 0);
    widget->setGeometry(rt);
}

void QWoShower::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    QRect rt(0, 0, width(), height());
    p.fillRect(rt, QColor(Qt::black));
    QFont ft = p.font();
    ft.setPixelSize(190);
    ft.setBold(true);
    p.setFont(ft);
    QPen pen = p.pen();
    pen.setStyle(Qt::DotLine);
    pen.setColor(Qt::lightGray);
    QBrush brush = pen.brush();
    brush.setStyle(Qt::Dense7Pattern);
    pen.setBrush(brush);
    p.setPen(pen);
    p.drawText(rt, Qt::AlignCenter, "WoTerm");
}

bool QWoShower::eventFilter(QObject *obj, QEvent *ev)
{
    switch (ev->type()) {
    case QEvent::MouseButtonPress:
        return tabMouseButtonPress((QMouseEvent*)ev);
    }
    return false;
}

void QWoShower::closeSession(int idx)
{
    if(idx >= m_tab->count()) {
        return;
    }
    QVariant v = m_tab->tabData(idx);
    QWoShowerWidget *target = v.value<QWoShowerWidget*>();
    target->deleteLater();
}

void QWoShower::createTab(QWoShowerWidget *impl, const QString& tabName)
{
    addWidget(impl);
    int idx = m_tab->addTab(tabName);
    m_tab->setCurrentIndex(idx);
    m_tab->setTabData(idx, QVariant::fromValue(impl));
    QObject::connect(impl, SIGNAL(destroyed(QObject*)), this, SLOT(onTermImplDestroy(QObject*)));
    setCurrentWidget(impl);
    qDebug() << "tabCount" << m_tab->count() << ",implCount" << count();
}

bool QWoShower::tabMouseButtonPress(QMouseEvent *ev)
{
    QPoint pt = ev->pos();
    int idx = m_tab->tabAt(pt);
    if(idx < 0) {
        emit openSessionManage();
        return false;
    }
    qDebug() << "tab hit" << idx;
    QVariant v = m_tab->tabData(idx);
    QWoShowerWidget *impl = v.value<QWoShowerWidget*>();
    bool ok = false;
    int type = impl->property(TAB_TYPE_NAME).toInt(&ok);
    if(ev->buttons().testFlag(Qt::RightButton)) {
        QMenu menu(impl);
        m_tabMenu = &menu;
        m_tabMenu->setProperty(TAB_TARGET_IMPL, QVariant::fromValue(impl));
        menu.addAction(tr("Close This Tab"), this, SLOT(onCloseThisTabSession()));
        menu.addAction(tr("Close Other Tab"), this, SLOT(onCloseOtherTabSession()));
        impl->handleTabContextMenu(&menu);
        menu.exec(QCursor::pos());
    }

    return false;
}

void QWoShower::onTabCloseRequested(int idx)
{
    QMessageBox::StandardButton btn = QMessageBox::warning(this, "CloseSession", "Close Or Not?", QMessageBox::Ok|QMessageBox::No);
    if(btn == QMessageBox::No) {
        return ;
    }
    closeSession(idx);
}

void QWoShower::onTabCurrentChanged(int idx)
{
    if(idx < 0) {
        return;
    }
    QVariant v = m_tab->tabData(idx);
    QWoShowerWidget *impl = v.value<QWoShowerWidget *>();
    setCurrentWidget(impl);
}

void QWoShower::onTermImplDestroy(QObject *it)
{
    QWidget *target = qobject_cast<QWidget*>(it);
    for(int i = 0; i < m_tab->count(); i++) {
        QVariant v = m_tab->tabData(i);
        QWidget *impl = v.value<QWidget *>();
        if(target == impl) {
            removeWidget(target);
            m_tab->removeTab(i);
            break;
        }
    }
    qDebug() << "tabCount" << m_tab->count() << ",implCount" << count();
}

void QWoShower::onTabbarDoubleClicked(int index)
{
    if(index < 0) {
        openLocalShell();
    }
}

void QWoShower::onCloseThisTabSession()
{
    if(m_tabMenu == nullptr) {
        return;
    }
    QVariant vimpl = m_tabMenu->property(TAB_TARGET_IMPL);
    QWidget *impl = vimpl.value<QWidget*>();
    if(impl == nullptr) {
        QMessageBox::warning(this, tr("alert"), tr("failed to find impl infomation"));
        return;
    }
    impl->deleteLater();
}

void QWoShower::onCloseOtherTabSession()
{
    if(m_tabMenu == nullptr) {
        return;
    }
    QVariant vimpl = m_tabMenu->property(TAB_TARGET_IMPL);
    QWidget *impl = vimpl.value<QWidget*>();
    if(impl == nullptr) {
        QMessageBox::warning(this, tr("alert"), tr("failed to find impl infomation"));
        return;
    }
    for(int i = 0; i < m_tab->count(); i++) {
        QVariant v = m_tab->tabData(i);
        QWidget *target = v.value<QWidget *>();
        if(target != impl) {
            target->deleteLater();
        }
    }
}
