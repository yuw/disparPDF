#ifndef GENERIC_HPP
#define GENERIC_HPP
/*
    Copyright © 2008-13 Qtrac Ltd. All rights reserved.
    This program or module is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 2 of
    the License, or (at your option) any later version. This program is
    distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
*/
#include <memory>
#include <vector>
#ifdef USE_QT6
#  include <poppler-qt6.h>
#else
#  include <poppler-qt5.h>
#endif
#include <QMetaType>
#include <QPair>
#include <QPixmap>
#include <QSet>

class QColor;
class QMimeData;
class QRectF;

// Qt6 Poppler: Document::load() / Page::page() return unique_ptr
typedef std::unique_ptr<Poppler::Document> PdfDocument;
// Page is still a raw pointer owned by Document
typedef Poppler::Page *PdfPage;
// Qt6 Poppler: textList() returns vector<unique_ptr<TextBox>>
typedef std::unique_ptr<Poppler::TextBox> PdfTextBox;
typedef std::vector<PdfTextBox> TextBoxList;

enum InitialComparisonMode{CompareAppearance=0, CompareCharacters=1,
                           CompareWords=2};

enum Debug{DebugOff, DebugShowTexts, DebugShowTextsAndYX};

// note: this should be a parameter
const int POINTS_PER_INCH = 72;

typedef QSet<int> Ranges;
typedef QPair<Ranges, Ranges> RangesPair;

struct PagePair
{
    PagePair(int l=-1, int r=-1, bool v=false)
        : left(l), right(r), hasVisualDifference(v) {}

    bool isNull() { return left == -1 || right == -1; }

    int left;
    int right;
    bool hasVisualDifference;
};
Q_DECLARE_METATYPE(PagePair)


inline const QChar canonicalizedCharacter(const QChar &in)
{
    QChar out = in;
    const ushort c = in.unicode();
    switch (c) {
        case 0x93:   out = QChar(0x201C); break; // "
        case 0x94:   out = QChar(0x201D); break; // "
        case 0xAD:   // fallthrough (soft-hyphen)
        case 0x2D:   // fallthrough (hyphen-minus)
        case 0x2010: // fallthrough (hyphen)
        case 0x2011: // fallthrough (non-breaking hyphen)
        case 0x2043: out = '-'; break; // (hyphen-bullet)
    }
    return out;
}


void scaleRect(int dpi, QRectF *rect);
int pointValueForPixelOffset(const double dpi, int px);
int pixelOffsetForPointValue(const double dpi, int pt);
QRectF rectForMargins(const int width, const int height, const int top,
        const int bottom, const int left, const int right);
Ranges unorderedRange(int end, int start=0);

QPixmap colorSwatch(const QColor &color);
QPixmap brushSwatch(const Qt::BrushStyle style, const QColor &color);
QPixmap penStyleSwatch(const Qt::PenStyle style, const QColor &color);

const TextBoxList getTextBoxes(PdfPage page, const QRectF &rect=QRect());

const QString strippedFilename(const QString &filename);
const QStringList droppedFilenames(const QMimeData *mimeData);
const QRectF resizeRect(const QRectF &pageRect, const QSize &pixmapSize);

#endif // GENERIC_HPP
