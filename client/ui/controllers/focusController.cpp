#include "focusController.h"
#include "utils/qmlUtils.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>

FocusController::FocusController(QQmlApplicationEngine *engine, QObject *parent)
    : QObject { parent },
      m_engine { engine },
      m_focusChain {},
      m_focusedItem { nullptr },
      m_rootObjects {},
      m_defaultFocusItem { QSharedPointer<QQuickItem>() },
      m_lvfc { nullptr }
{
    QObject::connect(m_engine.get(), &QQmlApplicationEngine::objectCreated, this,
                     [this](QObject *object, const QUrl &url) {
                         QQuickItem *newDefaultFocusItem = object->findChild<QQuickItem *>("defaultFocusItem");
                         if (newDefaultFocusItem && m_defaultFocusItem != newDefaultFocusItem) {
                             m_defaultFocusItem.reset(newDefaultFocusItem);
                             qDebug() << "===>> NEW DEFAULT FOCUS ITEM: " << m_defaultFocusItem;
                         }
                     });

    QObject::connect(this, &FocusController::focusedItemChanged, this,
                     [this]() { m_focusedItem->forceActiveFocus(Qt::TabFocusReason); });
}

void FocusController::nextKeyTabItem()
{
    nextItem(Direction::Forward);
}

void FocusController::previousKeyTabItem()
{
    nextItem(Direction::Backward);
}

void FocusController::nextKeyUpItem()
{
    nextItem(Direction::Backward);
}

void FocusController::nextKeyDownItem()
{
    nextItem(Direction::Forward);
}

void FocusController::nextKeyLeftItem()
{
    nextItem(Direction::Backward);
}

void FocusController::nextKeyRightItem()
{
    nextItem(Direction::Forward);
}

void FocusController::setFocusItem(QQuickItem *item)
{
    if (m_focusedItem != item) {
        m_focusedItem = item;
        qDebug() << "===>> FocusItem is changed to " << item << "!";
    } else {
        qDebug() << "===>> FocusItem is is the same: " << item << "!";
    }
    emit focusedItemChanged();
}

void FocusController::setFocusOnDefaultItem()
{
    qDebug() << "===>> Setting focus on DEFAULT FOCUS ITEM...";
    setFocusItem(m_defaultFocusItem.get());
}

void FocusController::pushRootObject(QObject *object)
{
    qDebug() << "===>> Calling < pushRootObject >...";
    m_rootObjects.push(object);
    dropListView();
    // setFocusOnDefaultItem();
    qDebug() << "===>> ROOT OBJECT is changed to: " << m_rootObjects.top();
    qDebug() << "===>> ROOT OBJECTS: " << m_rootObjects;
}

void FocusController::dropRootObject(QObject *object)
{
    qDebug() << "===>> Calling < dropRootObject >...";
    if (m_rootObjects.empty()) {
        qDebug() << "ROOT OBJECT is already DEFAULT";

        return;
    }

    if (m_rootObjects.top() == object) {
        m_rootObjects.pop();
        dropListView();
        setFocusOnDefaultItem();
        if (m_rootObjects.size()) {
            qDebug() << "===>> ROOT OBJECT is changed to: " << m_rootObjects.top();
        } else {
            qDebug() << "===>> ROOT OBJECT is changed to DEFAULT";
        }
    } else {
        qWarning() << "===>> TRY TO DROP WRONG ROOT OBJECT: " << m_rootObjects.top() << " SHOULD BE: " << object;
    }
}

void FocusController::resetRootObject()
{
    qDebug() << "===>> Calling < resetRootObject >...";
    m_rootObjects.clear();
    qDebug() << "===>> ROOT OBJECT IS RESETED";
}

void FocusController::reload(Direction direction)
{
    qDebug() << "===>> Calling < reload >...";
    m_focusChain.clear();

    QObject *rootObject = (m_rootObjects.empty() ? m_engine->rootObjects().value(0) : m_rootObjects.top());

    if (!rootObject) {
        qCritical() << "No ROOT OBJECT found!";
        resetRootObject();
        dropListView();
        return;
    }

    qDebug() << "===>> ROOT OBJECTS: " << rootObject;

    m_focusChain.append(focusControl::getSubChain(rootObject));

    std::sort(m_focusChain.begin(), m_focusChain.end(),
              direction == Direction::Forward ? focusControl::isLess : focusControl::isMore);

    if (m_focusChain.empty()) {
        qWarning() << "Focus chain is empty!";
        resetRootObject();
        dropListView();
        return;
    }
}

void FocusController::nextItem(Direction direction)
{
    qDebug() << "===>> Calling < nextItem >...";

    reload(direction);

    if (m_lvfc && focusControl::isListView(m_focusedItem)) {
        direction == Direction::Forward ? focusNextListViewItem() : focusPreviousListViewItem();
        qDebug() << "===>> Handling the [ ListView ]...";

        return;
    }

    if (m_focusChain.empty()) {
        qWarning() << "There are no items to navigate";
        setFocusOnDefaultItem();
        return;
    }

    auto focusedItemIndex = m_focusChain.indexOf(m_focusedItem);

    if (focusedItemIndex == -1) {
        qDebug() << "Current FocusItem is not in chain, switch to first in chain...";
        focusedItemIndex = 0;
    } else if (focusedItemIndex == (m_focusChain.size() - 1)) {
        qDebug() << "Last focus index. Starting from the beginning...";
        focusedItemIndex = 0;
    } else {
        qDebug() << "Incrementing focus index...";
        focusedItemIndex++;
    }

    const auto focusedItem = qobject_cast<QQuickItem *>(m_focusChain.at(focusedItemIndex));

    if (focusedItem == nullptr) {
        qWarning() << "Failed to get item to focus on. Setting focus on default";
        setFocusOnDefaultItem();
        return;
    }

    if (focusControl::isListView(focusedItem)) {
        qDebug() << "===>> Found [ListView]";
        m_lvfc = new ListViewFocusController(focusedItem, this);
        m_focusedItem = focusedItem;
        if (direction == Direction::Forward) {
            m_lvfc->nextDelegate();
            focusNextListViewItem();
        } else {
            m_lvfc->previousDelegate();
            focusPreviousListViewItem();
        }
        return;
    }

    setFocusItem(focusedItem);

    focusControl::printItems(m_focusChain, focusedItem);

    ///////////////////////////////////////////////////////////

    const auto w = m_defaultFocusItem->window();

    qDebug() << "===>> CURRENT ACTIVE ITEM: " << w->activeFocusItem();
    qDebug() << "===>> CURRENT FOCUS OBJECT: " << w->focusObject();
    if (m_rootObjects.empty()) {
        qDebug() << "===>> ROOT OBJECT IS DEFAULT";
    } else {
        qDebug() << "===>> ROOT OBJECT: " << m_rootObjects.top();
    }
}

void FocusController::focusNextListViewItem()
{
    qDebug() << "===>> Calling < focusNextListViewItem >...";

    if (m_lvfc->isLastFocusItemInListView() || m_lvfc->isReturnNeeded()) {
        qDebug() << "===>> Last item in [ ListView ] was reached. Going to the NEXT element after [ ListView ]";
        dropListView();
        nextItem(Direction::Forward);
        return;
    } else if (m_lvfc->isLastFocusItemInDelegate()) {
        qDebug() << "===>> End of delegate elements was reached. Going to the next delegate";
        m_lvfc->resetFocusChain();
        m_lvfc->nextDelegate();
    }

    m_lvfc->focusNextItem();
}

void FocusController::focusPreviousListViewItem()
{
    qDebug() << "===>> Calling < focusPreviousListViewItem >...";

    if (m_lvfc->isFirstFocusItemInListView() || m_lvfc->isReturnNeeded()) {
        qDebug() << "===>> First item in [ ListView ] was reached. Going to the PREVIOUS element after [ ListView ]";
        dropListView();
        nextItem(Direction::Backward);
        return;
    } else if (m_lvfc->isFirstFocusItemInDelegate()) {
        m_lvfc->resetFocusChain();
        m_lvfc->previousDelegate();
    }

    m_lvfc->focusPreviousItem();
}

void FocusController::dropListView()
{
    qDebug() << "===>> Calling < dropListView >...";

    if (m_lvfc) {
        delete m_lvfc;
        m_lvfc = nullptr;
    }
}
