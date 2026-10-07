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

#include "renderer.h"
#include "aboutform.hpp"
#include "sequence_matcher.hpp"
#include "textitem.hpp"
#include <QFont>
#include <algorithm>
#include <QImage>
#include <QPageLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPrinter>
#include <QTextOption>


int RenderSettings::dpi() const
{
    return static_cast<int>(POINTS_PER_INCH * (zoom / 100.0));
}


QRectF RenderSettings::pointRectForMargins(const QSize &pageSize) const
{
    return rectForMargins(pageSize.width(), pageSize.height(), topMargin,
                          bottomMargin, leftMargin, rightMargin);
}


QRect RenderSettings::pixelRectForMargins(const QSize &imageSize) const
{
    const int DPI = dpi();
    int top = pixelOffsetForPointValue(DPI, topMargin);
    int left = pixelOffsetForPointValue(DPI, leftMargin);
    int right = pixelOffsetForPointValue(DPI, rightMargin);
    int bottom = pixelOffsetForPointValue(DPI, bottomMargin);
    return QRect(QPoint(left, top), QPoint(imageSize.width() - right,
                                           imageSize.height() - bottom));
}


DifferenceRenderer::DifferenceRenderer(const RenderSettings &settings)
    : settings(settings)
{
}


QPair<QString, QString> DifferenceRenderer::cacheKeys(
        const QString &filename1, const QString &filename2,
        const PagePair &pair) const
{
    QString zoning;
    if (settings.zoning)
        zoning = QString("%1:%2:%3").arg(settings.columns)
                .arg(settings.toleranceR).arg(settings.toleranceY);
    QString margins;
    if (settings.excludeMargins)
        margins = QString("%1:%2:%3:%4").arg(settings.topMargin)
                .arg(settings.bottomMargin).arg(settings.leftMargin)
                .arg(settings.rightMargin);
    const QString highlighting = QString("%1:%2:%3:%4:%5:%6:%7:%8")
            .arg(settings.pen.color().name()).arg(settings.pen.style())
            .arg(settings.brush.style()).arg(settings.opacity)
            .arg(settings.squareSize).arg(settings.ruleWidth)
            .arg(settings.overlap).arg(settings.combineTextHighlighting);
    const QString key = QString("%1%2:%3:%4:%5:%6:%7:%8")
            .arg(pair.differs ? "D" : "S")
            .arg(pair.hasVisualDifference ? "V" : "T")
            .arg(settings.zoom).arg(settings.compareMode)
            .arg(settings.compositionMode).arg(zoning).arg(margins)
            .arg(highlighting);
    const QString key1 = QString("1:%1:%2:%3").arg(key).arg(pair.left)
            .arg(filename1);
    const QString key2 = QString("2:%1:%2:%3").arg(key).arg(pair.right)
            .arg(filename2);
    return qMakePair(key1, key2);
}


QPair<QPixmap, QPixmap> DifferenceRenderer::pixmaps(const PdfPage &page1,
        const QString &filename1, const PdfPage &page2,
        const QString &filename2, const PagePair &pair) const
{
    const QPair<QString, QString> keys = cacheKeys(filename1, filename2,
                                                   pair);
    QPixmap pixmap1;
    QPixmap pixmap2;
    if (QPixmapCache::find(keys.first, &pixmap1) &&
        QPixmapCache::find(keys.second, &pixmap2))
        return qMakePair(pixmap1, pixmap2);

    const int DPI = settings.dpi();
    const bool compareText = settings.compareMode != CompareAppearance;
    QImage plainImage1;
    QImage plainImage2;
    if (pair.hasVisualDifference || !compareText) {
        plainImage1 = page1->renderToImage(DPI, DPI);
        plainImage2 = page2->renderToImage(DPI, DPI);
    }
    QImage image1 = page1->renderToImage(DPI, DPI);
    QImage image2 = page2->renderToImage(DPI, DPI);

    if (compareText || settings.compositionMode == -1) {
        QPainterPath highlighted1;
        QPainterPath highlighted2;
        if (pair.hasVisualDifference || !compareText)
            computeVisualHighlights(&highlighted1, &highlighted2,
                    plainImage1, plainImage2);
        else
            computeTextHighlights(&highlighted1, &highlighted2, page1,
                    page2, DPI);
        if (!highlighted1.isEmpty())
            paintOnImage(highlighted1, &image1);
        if (!highlighted2.isEmpty())
            paintOnImage(highlighted2, &image2);
        if (pair.differs && highlighted1.isEmpty() &&
            highlighted2.isEmpty()) {
            QFont font("Helvetica", 14);
            font.setOverline(true);
            font.setUnderline(true);
            highlighted1.addText(DPI / 4, DPI / 4, font,
                tr("%1: False Positive").arg(AboutForm::ProgramName));
            paintOnImage(highlighted1, &image1);
        }
        pixmap1 = QPixmap::fromImage(image1);
        pixmap2 = QPixmap::fromImage(image2);
    } else {
        pixmap1 = QPixmap::fromImage(image1);
        QImage composed(image1.size(), image1.format());
        QPainter painter(&composed);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(composed.rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(0, 0, image1);
        painter.setCompositionMode(static_cast<QPainter::CompositionMode>(
                settings.compositionMode));
        painter.drawImage(0, 0, image2);
        painter.setCompositionMode(
                QPainter::CompositionMode_DestinationOver);
        painter.fillRect(composed.rect(), Qt::white);
        painter.end();
        pixmap2 = QPixmap::fromImage(composed);
    }
    QPixmapCache::insert(keys.first, pixmap1);
    QPixmapCache::insert(keys.second, pixmap2);
    return qMakePair(pixmap1, pixmap2);
}


void DifferenceRenderer::computeTextHighlights(QPainterPath *highlighted1,
        QPainterPath *highlighted2, const PdfPage &page1,
        const PdfPage &page2, const int DPI) const
{
    const bool ComparingWords = settings.compareMode == CompareWords;
    QRectF rect1;
    QRectF rect2;
    QRectF rect;
    if (settings.excludeMargins)
        rect = settings.pointRectForMargins(page1->pageSize());
    const TextBoxList list1 = getTextBoxes(page1, rect);
    const TextBoxList list2 = getTextBoxes(page2, rect);
    TextItems items1 = ComparingWords ? getWords(list1)
                                      : getCharacters(list1);
    TextItems items2 = ComparingWords ? getWords(list2)
                                      : getCharacters(list2);
    const int ToleranceY = settings.toleranceY;
    if (settings.zoning) {
        const int ToleranceR = settings.toleranceR;
        const int Columns = settings.columns;
        items1.columnZoneYxOrder(page1->pageSize().width(), ToleranceR,
                                 ToleranceY, Columns);
        items2.columnZoneYxOrder(page2->pageSize().width(), ToleranceR,
                                 ToleranceY, Columns);
    }

    if (settings.debug >= DebugShowTexts) {
        const bool Yx = settings.debug == DebugShowTextsAndYX;
        items1.debug(1, ToleranceY, ComparingWords, Yx);
        items2.debug(2, ToleranceY, ComparingWords, Yx);
    }

    SequenceMatcher matcher(items1.texts(), items2.texts());
    RangesPair rangesPair = computeRanges(&matcher);
    rangesPair = invertRanges(rangesPair.first, items1.count(),
                              rangesPair.second, items2.count());

    // Highlight in reading order, so that adjacent differences are
    // combined; a QSet's order depends on Qt's per-process hash seed
    QList<int> indexes1 = rangesPair.first.values();
    QList<int> indexes2 = rangesPair.second.values();
    std::sort(indexes1.begin(), indexes1.end());
    std::sort(indexes2.begin(), indexes2.end());
    for (int index : indexes1)
        addHighlighting(&rect1, highlighted1, items1.at(index).rect, DPI);
    if (!rect1.isNull() && !indexes1.isEmpty())
        highlighted1->addRect(rect1);
    for (int index : indexes2)
        addHighlighting(&rect2, highlighted2, items2.at(index).rect, DPI);
    if (!rect2.isNull() && !indexes2.isEmpty())
        highlighted2->addRect(rect2);
}


void DifferenceRenderer::addHighlighting(QRectF *bigRect,
        QPainterPath *highlighted, const QRectF wordOrCharRect,
        const int DPI) const
{
    const int OVERLAP = settings.overlap;
    QRectF rect = wordOrCharRect;
    scaleRect(DPI, &rect);
    if (settings.combineTextHighlighting &&
        rect.adjusted(-OVERLAP, -OVERLAP, OVERLAP, OVERLAP)
        .intersects(*bigRect))
        *bigRect = bigRect->united(rect);
    else {
        highlighted->addRect(*bigRect);
        *bigRect = rect;
    }
}


void DifferenceRenderer::computeVisualHighlights(
        QPainterPath *highlighted1, QPainterPath *highlighted2,
        const QImage &plainImage1, const QImage &plainImage2) const
{
    const int SQUARE_SIZE = settings.squareSize;
    QRect box;
    if (settings.excludeMargins)
        box = settings.pixelRectForMargins(plainImage1.size());
    QRect target;
    for (int x = 0; x < plainImage1.width(); x += SQUARE_SIZE) {
        for (int y = 0; y < plainImage1.height(); y += SQUARE_SIZE) {
            const QRect rect(x, y, SQUARE_SIZE, SQUARE_SIZE);
            if (!box.isEmpty() && !box.contains(rect))
                continue;
            QImage temp1 = plainImage1.copy(rect);
            QImage temp2 = plainImage2.copy(rect);
            if (temp1 != temp2) {
                if (rect.adjusted(-1, -1, 1, 1).intersects(target))
                    target = target.united(rect);
                else {
                    highlighted1->addRect(target);
                    highlighted2->addRect(target);
                    target = rect;
                }
            }
        }
    }
    if (!target.isNull()) {
        highlighted1->addRect(target);
        highlighted2->addRect(target);
    }
}


void DifferenceRenderer::paintOnImage(const QPainterPath &path,
                                      QImage *image) const
{
    const QPen &pen = settings.pen;
    QPen pen_(pen);
    QBrush brush_(settings.brush);
    QColor color = pen.color();
    color.setAlphaF(settings.opacity / 100.0);
    pen_.setColor(color);
    brush_.setColor(color);

    QPainter painter(image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(pen_);
    painter.setBrush(brush_);

    const int SQUARE_SIZE = settings.squareSize;
    const double RULE_WIDTH = settings.ruleWidth;
    QRectF rect = path.boundingRect();
    if (rect.width() < SQUARE_SIZE && rect.height() < SQUARE_SIZE) {
        rect.setHeight(SQUARE_SIZE);
        rect.setWidth(SQUARE_SIZE);
        painter.drawRect(rect);
        if (!qFuzzyCompare(RULE_WIDTH, 0.0)) {
            painter.setPen(QPen(pen.color()));
            painter.drawRect(0, rect.y(), RULE_WIDTH, rect.height());
        }
    }
    else {
        QPainterPath path_(path);
        path_.setFillRule(Qt::WindingFill);
        painter.drawPath(path_);
        if (!qFuzzyCompare(RULE_WIDTH, 0.0)) {
            painter.setPen(QPen(pen.color()));
            QList<QPolygonF> polygons = path_.toFillPolygons();
            for (const QPolygonF &polygon : polygons) {
                const QRectF rect = polygon.boundingRect();
                painter.drawRect(0, rect.y(), RULE_WIDTH, rect.height());
            }
        }
    }
    painter.end();
}


bool DifferenceRenderer::paintPair(QPainter *painter,
        const PdfDocument &pdf1, const QString &filename1,
        const PdfDocument &pdf2, const QString &filename2,
        const PagePair &pair, const QString &header,
        const SavePages savePages, const QRectF &rect,
        const QRectF &leftRect, const QRectF &rightRect) const
{
    if (pair.left < 0 || pair.right < 0)
        return false;
    PdfPage page1 = pdf1->page(pair.left);
    if (!page1)
        return false;
    PdfPage page2 = pdf2->page(pair.right);
    if (!page2)
        return false;
    const QPair<QPixmap, QPixmap> images = pixmaps(page1, filename1,
            page2, filename2, pair);
    if (!header.isEmpty())
        painter->drawText(rect, header, QTextOption(Qt::AlignHCenter|
                                                    Qt::AlignTop));
    if (savePages == SaveBothPages) {
        QRectF rect = resizeRect(leftRect, images.first.size());
        painter->drawPixmap(rect.toAlignedRect(), images.first);
        rect = resizeRect(rightRect, images.second.size());
        painter->drawPixmap(rect.toAlignedRect(), images.second);
        painter->drawRect(rightRect.adjusted(2.5, 2.5, 2.5, 2.5));
    } else if (savePages == SaveLeftPages) {
        QRectF rect = resizeRect(leftRect, images.first.size());
        painter->drawPixmap(rect.toAlignedRect(), images.first);
    } else { // (savePages == SaveRightPages)
        QRectF rect = resizeRect(leftRect, images.second.size());
        painter->drawPixmap(rect.toAlignedRect(), images.second);
    }
    painter->drawRect(leftRect.adjusted(2.5, 2.5, 2.5, 2.5));
    return true;
}


bool DifferenceRenderer::saveAsPdf(const QString &outputFile,
        const PdfDocument &pdf1, const QString &filename1,
        const PdfDocument &pdf2, const QString &filename2,
        const QList<PagePair> &pairs, const QString &header,
        const SavePages savePages) const
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFileName(outputFile);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setColorMode(QPrinter::Color);
    printer.setCreator(AboutForm::ProgramName);
    printer.setPageOrientation(savePages == SaveBothPages
            ? QPageLayout::Landscape : QPageLayout::Portrait);
    QPainter painter(&printer);
    painter.setRenderHints(QPainter::Antialiasing|
            QPainter::TextAntialiasing|QPainter::SmoothPixmapTransform);
    painter.setFont(QFont("Helvetica", 11));
    painter.setPen(Qt::darkCyan);
    const QRect rect(0, 0, painter.viewport().width(),
                        painter.fontMetrics().height());
    const int y = painter.fontMetrics().lineSpacing();
    const int height = painter.viewport().height() - y;
    const int gap = 30;
    int width = (painter.viewport().width() / 2) - gap;
    if (savePages != SaveBothPages)
        width = painter.viewport().width();
    const QRect leftRect(0, y, width, height);
    const QRect rightRect(width + gap, y, width, height);
    bool ok = true;
    for (int i = 0; i < pairs.count(); ++i) {
        if (!paintPair(&painter, pdf1, filename1, pdf2, filename2,
                       pairs.at(i), header, savePages, rect, leftRect,
                       rightRect)) {
            ok = false;
            continue;
        }
        if (i + 1 < pairs.count())
            printer.newPage();
    }
    painter.end();
    return ok && printer.printerState() != QPrinter::Error;
}


PdfDocument loadPdf(const QString &filename, bool *locked)
{
    *locked = false;
    PdfDocument pdf(Poppler::Document::load(filename));
    if (pdf && pdf->isLocked()) {
        *locked = true;
        pdf.reset();
    }
    if (pdf) {
        // Compare and highlight pages as they are displayed: without
        // antialiasing, some visible differences (such as text printed
        // twice in the same place, which looks bolder) render identically.
        pdf->setRenderHint(Poppler::Document::Antialiasing);
        pdf->setRenderHint(Poppler::Document::TextAntialiasing);
    }
    return pdf;
}
