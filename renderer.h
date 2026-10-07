#ifndef RENDERER_H
#define RENDERER_H
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

#include "generic.hpp"
#include "saveform.hpp"
#include <QBrush>
#include <QCoreApplication>
#include <QList>
#include <QPair>
#include <QPen>
#include <QPixmap>
#include <QRect>
#include <QRectF>
#include <QString>

class QImage;
class QPainter;
class QPainterPath;

// Everything that affects how differences are found and shown.  The GUI
// fills this in from its widgets and settings, batch mode from its
// settings file.
struct RenderSettings
{
    int compareMode = CompareAppearance;  // an InitialComparisonMode
    int zoom = 100;                       // percent
    bool zoning = false;
    int columns = 1;
    int toleranceR = 8;
    int toleranceY = 10;
    bool excludeMargins = false;
    int topMargin = 0;                    // points
    int bottomMargin = 0;
    int leftMargin = 0;
    int rightMargin = 0;
    // In Appearance mode: -1 to highlight the differences, or the
    // QPainter::CompositionMode with which to show page 2 over page 1
    int compositionMode = -1;
    QPen pen;
    QBrush brush;
    int opacity = 13;                     // percent
    int squareSize = 10;                  // pixels
    double ruleWidth = 1.5;
    int overlap = 5;
    bool combineTextHighlighting = true;
    Debug debug = DebugOff;

    int dpi() const;                      // at the zoom
    QRectF pointRectForMargins(const QSize &pageSize) const;
    QRect pixelRectForMargins(const QSize &imageSize) const;
};

// Renders pairs of pages with their differences highlighted (or, in
// Appearance mode, page 2 composed over page 1), for display and for
// saving.  Rendered pixmaps are kept in QPixmapCache.
class DifferenceRenderer
{
    Q_DECLARE_TR_FUNCTIONS(DifferenceRenderer)

public:
    explicit DifferenceRenderer(const RenderSettings &settings);

    // The two pages of pair, rendered at the zoom
    QPair<QPixmap, QPixmap> pixmaps(const PdfPage &page1,
            const QString &filename1, const PdfPage &page2,
            const QString &filename2, const PagePair &pair) const;

    // Paints pair, with header above it, into leftRect and rightRect (or
    // only leftRect if savePages is not SaveBothPages); false if a page
    // cannot be read
    bool paintPair(QPainter *painter, const PdfDocument &pdf1,
            const QString &filename1, const PdfDocument &pdf2,
            const QString &filename2, const PagePair &pair,
            const QString &header, const SavePages savePages,
            const QRectF &rect, const QRectF &leftRect,
            const QRectF &rightRect) const;

    // Saves the pairs to outputFile, one per page; false if a pair could
    // not be painted or the file could not be written
    bool saveAsPdf(const QString &outputFile, const PdfDocument &pdf1,
            const QString &filename1, const PdfDocument &pdf2,
            const QString &filename2, const QList<PagePair> &pairs,
            const QString &header, const SavePages savePages) const;

private:
    QPair<QString, QString> cacheKeys(const QString &filename1,
            const QString &filename2, const PagePair &pair) const;
    void computeTextHighlights(QPainterPath *highlighted1,
            QPainterPath *highlighted2, const PdfPage &page1,
            const PdfPage &page2, const int DPI) const;
    void addHighlighting(QRectF *bigRect, QPainterPath *highlighted,
            const QRectF wordOrCharRect, const int DPI) const;
    void computeVisualHighlights(QPainterPath *highlighted1,
            QPainterPath *highlighted2, const QImage &plainImage1,
            const QImage &plainImage2) const;
    void paintOnImage(const QPainterPath &path, QImage *image) const;

    const RenderSettings settings;
};

// Loads a PDF to compare and show, rendered with antialiasing; null if it
// cannot be read, in which case *locked says whether it was because it is
// locked
PdfDocument loadPdf(const QString &filename, bool *locked);

#endif // RENDERER_H
