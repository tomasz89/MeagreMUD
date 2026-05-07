#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>

// ---------------------------------------------------------------------------
// StyledRun — a span of text with a single consistent style.
//
// fg/bg are indices into the 16-colour ANSI palette (8 normal + 8 bright).
// 0xFF means "default terminal colour".
//
// Wire format: [fg: 1][bg: 1][bold: 1][text_len: 2][text: N (UTF-8)]
// ---------------------------------------------------------------------------

static constexpr quint8 COLOUR_DEFAULT = 0xFF;

struct StyledRun {
    quint8  fg   = COLOUR_DEFAULT;
    quint8  bg   = COLOUR_DEFAULT;
    bool    bold = false;
    QString text;
};

// ---------------------------------------------------------------------------
// Line — a vector of StyledRuns forming one display line.
// The last Line in the backbuffer is always open/in-progress.
// Lines are closed only when \n is encountered within a run's text.
// ---------------------------------------------------------------------------

using Line = QVector<StyledRun>;
