#pragma once

#include <QWidget>

// ---------------------------------------------------------------------------
// TiledArea
//
// Overview grid showing multiple CharacterPane instances simultaneously,
// like a video-feed grid. Only visible when at least one pane is tiled.
// Each pane is sized to an equal fraction of the available area, with height
// snapped to whole display lines.
//
// This is a placeholder skeleton — layout logic is implemented once
// CharacterPane and TerminalWidget are complete.
// ---------------------------------------------------------------------------

class TiledArea : public QWidget
{
    Q_OBJECT

public:
    explicit TiledArea(QWidget *parent = nullptr);

    int paneCount() const { return m_paneCount; }

private:
    int m_paneCount = 0;
};
