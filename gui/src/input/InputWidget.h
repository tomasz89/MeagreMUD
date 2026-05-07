#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QStringList>

// ---------------------------------------------------------------------------
// InputWidget
//
// Single-line text input bar for a character pane. Sits below the terminal.
//
// Behaviour:
//   - Return / Enter submits the current text, appends \r\n, and clears
//     the input field.
//   - Submitted text is added to a local history ring (up to HISTORY_SIZE
//     entries). Up/Down arrow keys navigate history exactly as a shell would:
//     Up recalls older entries, Down returns toward the current draft.
//   - While locked (resync, observer mode) the field is visually disabled
//     and ignores Return/Enter.
//   - Key macros: if the key combination matches a registered macro the
//     macro text is sent immediately without touching the input field or
//     history. Macro registration is handled externally via addMacro().
//
// The widget does NOT send anything to the network directly. It emits
// textSubmitted(QString) and the owning CharacterPane / MainWindow forwards
// it to DaemonConnectionManager.
// ---------------------------------------------------------------------------

class InputWidget : public QWidget
{
    Q_OBJECT

public:
    static constexpr int HISTORY_SIZE = 100;

    explicit InputWidget(QWidget *parent = nullptr);

    // Lock/unlock the input field.
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

    // Register a key macro. key and modifiers are Qt::Key and
    // Qt::KeyboardModifiers values. When the combination fires, macroText
    // is emitted via textSubmitted() directly.
    void addMacro(int key, Qt::KeyboardModifiers modifiers,
                  const QString &macroText);
    void clearMacros();

    // Programmatically set the input text (e.g. paste from attention panel).
    void setText(const QString &text);

signals:
    // Emitted when the user submits text or a macro fires.
    // Text has \r\n appended.
    void textSubmitted(const QString &text);

private slots:
    void onReturnPressed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Macro {
        int                    key;
        Qt::KeyboardModifiers  modifiers;
        QString                text;
    };

    void historyUp();
    void historyDown();
    void submitText(const QString &text);

    QLineEdit       *m_edit         = nullptr;
    bool             m_locked       = false;

    // Input history
    QStringList      m_history;
    int              m_historyIndex = -1;   // -1 = not browsing
    QString          m_draft;               // saved current line while browsing

    // Key macros
    QVector<Macro>   m_macros;
};
