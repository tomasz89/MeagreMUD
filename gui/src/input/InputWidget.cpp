#include "input/InputWidget.h"

#include <QHBoxLayout>
#include <QKeyEvent>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

InputWidget::InputWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    m_edit = new QLineEdit(this);
    m_edit->setFrame(false);
    m_edit->setPlaceholderText(QStringLiteral("Enter command..."));
    m_edit->installEventFilter(this);
    layout->addWidget(m_edit);

    connect(m_edit, &QLineEdit::returnPressed,
            this, &InputWidget::onReturnPressed);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void InputWidget::setLocked(bool locked)
{
    m_locked = locked;
    m_edit->setEnabled(!locked);
    if (locked)
    {
        m_edit->setPlaceholderText(QStringLiteral("(locked)"));
    }
    else
    {
        m_edit->setPlaceholderText(QStringLiteral("Enter command..."));
    }
}

void InputWidget::addMacro(int key, Qt::KeyboardModifiers modifiers,
                           const QString &macroText)
{
    Macro m;
    m.key       = key;
    m.modifiers = modifiers;
    m.text      = macroText;
    m_macros.append(m);
}

void InputWidget::clearMacros()
{
    m_macros.clear();
}

void InputWidget::setText(const QString &text)
{
    m_edit->setText(text);
    m_edit->setFocus();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void InputWidget::onReturnPressed()
{
    if (m_locked)
    {
        return;
    }

    const QString text = m_edit->text();
    m_edit->clear();
    m_historyIndex = -1;
    m_draft.clear();
    submitText(text);
}

// ---------------------------------------------------------------------------
// Event filter  -  intercepts key events on the QLineEdit
// ---------------------------------------------------------------------------

bool InputWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_edit)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() != QEvent::KeyPress)
    {
        return false;
    }

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    // Check macros first  -  they fire even when not locked
    for (const Macro &macro : m_macros)
    {
        if (keyEvent->key() == macro.key
            && keyEvent->modifiers() == macro.modifiers)
        {
            submitText(macro.text);
            return true;
        }
    }

    if (m_locked)
    {
        return false;
    }

    if (keyEvent->key() == Qt::Key_Up)
    {
        historyUp();
        return true;
    }

    if (keyEvent->key() == Qt::Key_Down)
    {
        historyDown();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// History navigation
// ---------------------------------------------------------------------------

void InputWidget::historyUp()
{
    if (m_history.isEmpty())
    {
        return;
    }

    if (m_historyIndex == -1)
    {
        // Save the current draft before starting to browse
        m_draft = m_edit->text();
        m_historyIndex = m_history.size() - 1;
    }
    else if (m_historyIndex > 0)
    {
        m_historyIndex--;
    }

    m_edit->setText(m_history.at(m_historyIndex));
    m_edit->end(false);
}

void InputWidget::historyDown()
{
    if (m_historyIndex == -1)
    {
        return;
    }

    if (m_historyIndex < m_history.size() - 1)
    {
        m_historyIndex++;
        m_edit->setText(m_history.at(m_historyIndex));
    }
    else
    {
        // Reached the bottom  -  restore the draft
        m_historyIndex = -1;
        m_edit->setText(m_draft);
        m_draft.clear();
    }

    m_edit->end(false);
}

// ---------------------------------------------------------------------------
// Submit
// ---------------------------------------------------------------------------

void InputWidget::submitText(const QString &text)
{
    if (text.isEmpty())
    {
        return;
    }

    // Add to history  -  deduplicate consecutive identical entries
    if (m_history.isEmpty() || m_history.last() != text)
    {
        m_history.append(text);
        if (m_history.size() > HISTORY_SIZE)
        {
            m_history.removeFirst();
        }
    }

    emit textSubmitted(text + QStringLiteral("\r\n"));
}
