#include "window/AttentionPanel.h"

#include <QHBoxLayout>
#include <QScrollArea>

AttentionPanel::AttentionPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Toggle button  -  always visible, acts as the collapsed header
    m_toggleButton = new QPushButton(this);
    m_toggleButton->setFlat(true);
    m_toggleButton->setStyleSheet(QStringLiteral(
        "text-align: left; padding: 2px 6px;"));
    connect(m_toggleButton, &QPushButton::clicked,
            this, &AttentionPanel::toggleExpanded);
    outerLayout->addWidget(m_toggleButton);

    // Collapsible drawer
    m_drawer = new QWidget(this);
    m_drawerLayout = new QVBoxLayout(m_drawer);
    m_drawerLayout->setContentsMargins(4, 4, 4, 4);
    m_drawerLayout->setSpacing(2);
    m_drawerLayout->addStretch();
    outerLayout->addWidget(m_drawer);

    setExpanded(false);
    updateBadge();
}

void AttentionPanel::addEvent(quint8 characterId, quint16 eventId,
                              quint8 eventType, const QString &text)
{
    Q_UNUSED(eventType)

    // Build a row widget: [char id label] [description] [Dismiss]
    auto *row    = new QWidget(m_drawer);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(2, 2, 2, 2);

    auto *charLabel = new QLabel(
        QStringLiteral("Char %1").arg(characterId), row);
    charLabel->setFixedWidth(60);
    layout->addWidget(charLabel);

    auto *descLabel = new QLabel(text, row);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel, 1);

    auto *dismissBtn = new QPushButton(QStringLiteral("Dismiss"), row);
    // Capture by value  -  safe because the button lifetime is tied to the row
    connect(dismissBtn, &QPushButton::clicked, this,
            [this, characterId, eventId]()
            {
                emit dismissRequested(characterId, eventId);
            });
    layout->addWidget(dismissBtn);

    row->setProperty("characterId", characterId);
    row->setProperty("eventId",     eventId);

    // Insert before the stretch at the end
    m_drawerLayout->insertWidget(m_drawerLayout->count() - 1, row);

    m_eventCount++;
    updateBadge();

    if (m_eventCount == 1)
    {
        setExpanded(true);
    }
}

void AttentionPanel::removeEvent(quint8 characterId, quint16 eventId)
{
    for (int i = 0; i < m_drawerLayout->count(); ++i)
    {
        QLayoutItem *item = m_drawerLayout->itemAt(i);
        if (item == nullptr)
        {
            continue;
        }
        QWidget *w = item->widget();
        if (w == nullptr)
        {
            continue;
        }
        if (w->property("characterId").toUInt() == characterId
            && w->property("eventId").toUInt() == eventId)
        {
            m_drawerLayout->removeItem(item);
            delete w;
            m_eventCount--;
            break;
        }
    }

    updateBadge();

    if (m_eventCount == 0)
    {
        setExpanded(false);
    }
}

void AttentionPanel::clearCharacter(quint8 characterId)
{
    for (int i = m_drawerLayout->count() - 1; i >= 0; --i)
    {
        QLayoutItem *item = m_drawerLayout->itemAt(i);
        if (item == nullptr)
        {
            continue;
        }
        QWidget *w = item->widget();
        if (w == nullptr)
        {
            continue;
        }
        if (w->property("characterId").toUInt() == characterId)
        {
            m_drawerLayout->removeItem(item);
            delete w;
            m_eventCount--;
        }
    }

    updateBadge();

    if (m_eventCount == 0)
    {
        setExpanded(false);
    }
}

void AttentionPanel::toggleExpanded()
{
    setExpanded(!m_expanded);
}

void AttentionPanel::updateBadge()
{
    if (m_eventCount == 0)
    {
        m_toggleButton->setText(QStringLiteral("[!] No issues"));
    }
    else if (m_eventCount == 1)
    {
        m_toggleButton->setText(QStringLiteral("[!] 1 issue v"));
    }
    else
    {
        m_toggleButton->setText(
            QStringLiteral("[!] %1 issues v").arg(m_eventCount));
    }
}

void AttentionPanel::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_drawer->setVisible(expanded);
}
