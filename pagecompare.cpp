/*
    Copyright © 2026 Barak A. Pearlmutter. All rights reserved.
    This program or module is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 2 of
    the License, or (at your option) any later version. This program is
    distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
*/

#include "pagecompare.h"
#include <QFuture>
#include <QImage>
#include <QMultiHash>
#include <QThread>
#include <QtConcurrent>


// True if each word in one list matches a word with the same text in the
// other whose bounding box is within tolerance points of its own.
// Poppler's reading order can change when text moves by a tiny fraction
// of a point (e.g., after rewriting a PDF with Ghostscript), so the same
// words in the same places can come out in a different order.
static bool sameWordsInSamePlaces(const TextBoxList &list1,
                                  const TextBoxList &list2,
                                  const qreal tolerance)
{
    if (list1.size() != list2.size())
        return false;
    QMultiHash<QString, size_t> index;
    for (size_t i = 0; i < list2.size(); ++i)
        index.insert(list2[i]->text(), i);
    std::vector<bool> used(list2.size(), false);
    for (const PdfTextBox &box : list1) {
        const QString text = box->text();
        const QRectF rect = box->boundingBox();
        bool found = false;
        for (auto it = index.find(text);
             it != index.end() && it.key() == text; ++it) {
            const QRectF other = list2[it.value()]->boundingBox();
            if (!used[it.value()] &&
                qAbs(other.left() - rect.left()) <= tolerance &&
                qAbs(other.top() - rect.top()) <= tolerance &&
                qAbs(other.right() - rect.right()) <= tolerance &&
                qAbs(other.bottom() - rect.bottom()) <= tolerance) {
                used[it.value()] = true;
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}


PageDifference comparePagePair(const PdfPage &page1, const PdfPage &page2,
                               const PageCompareOptions &options)
{
    const QSize size = page1->pageSize();
    QRectF rect;
    if (options.excludeMargins)
        rect = rectForMargins(size.width(), size.height(),
                options.topMargin, options.bottomMargin,
                options.leftMargin, options.rightMargin);
    const TextBoxList list1 = getTextBoxes(page1, rect);
    const TextBoxList list2 = getTextBoxes(page2, rect);
    if (list1.size() != list2.size())
        return TextualPageDifference;
    for (size_t i = 0; i < list1.size(); ++i)
        if (list1[i]->text() != list2[i]->text()) {
            // The words may just be in a different order
            const qreal Tolerance = 0.1; // points
            if (!sameWordsInSamePlaces(list1, list2, Tolerance))
                return TextualPageDifference;
            break;
        }

    if (options.compareAppearance) {
        const int DPI = POINTS_PER_INCH;
        int x = -1;
        int y = -1;
        int width = -1;
        int height = -1;
        if (options.excludeMargins) {
            x = pixelOffsetForPointValue(DPI, options.leftMargin);
            y = pixelOffsetForPointValue(DPI, options.topMargin);
            width = pixelOffsetForPointValue(DPI, size.width() -
                    (options.leftMargin + options.rightMargin));
            height = pixelOffsetForPointValue(DPI, size.height() -
                    (options.topMargin + options.bottomMargin));
        }
        const QImage image1 = page1->renderToImage(DPI, DPI, x, y,
                                                   width, height);
        const QImage image2 = page2->renderToImage(DPI, DPI, x, y,
                                                   width, height);
        if (image1 != image2)
            return VisualPageDifference;
    }
    return NoPageDifference;
}


// Every Poppler::Document::RenderHint, listed explicitly rather than
// walked by bit-shifting: that assumed the hints occupy consecutive bits
// and that HideAnnotations is the highest one, so a hint added above it
// would have been copied silently.  Add new hints here.
static const Poppler::Document::RenderHint AllRenderHints[] = {
    Poppler::Document::Antialiasing,
    Poppler::Document::TextAntialiasing,
    Poppler::Document::TextHinting,
    Poppler::Document::TextSlightHinting,
    Poppler::Document::OverprintPreview,
    Poppler::Document::ThinLineSolid,
    Poppler::Document::ThinLineShape,
    Poppler::Document::IgnorePaperColor,
    Poppler::Document::HideAnnotations,
};


static PdfDocument loadCopy(const QString &filename, const PdfDocument &model)
{
    PdfDocument pdf(Poppler::Document::load(filename));
    if (!pdf || pdf->isLocked())
        return PdfDocument();
    const Poppler::Document::RenderHints hints = model->renderHints();
    for (const Poppler::Document::RenderHint hint : AllRenderHints)
        pdf->setRenderHint(hint, hints.testFlag(hint));
    return pdf;
}


QVector<PagePairResult> comparePagesInParallel(
        const QString &filename1, const PdfDocument &pdf1,
        const QList<int> &pages1,
        const QString &filename2, const PdfDocument &pdf2,
        const QList<int> &pages2,
        const PageCompareOptions &options,
        const std::atomic<bool> *cancel,
        const std::function<void(int)> &progress)
{
    const int total = qMin(pages1.count(), pages2.count());
    QVector<PagePairResult> results(total);
    // Workers write through this rather than QVector::operator[], which
    // may detach
    PagePairResult *const resultData = results.data();
    std::atomic<int> next(0);
    std::atomic<int> done(0);

    auto worker = [&]() {
        const PdfDocument doc1 = loadCopy(filename1, pdf1);
        const PdfDocument doc2 = loadCopy(filename2, pdf2);
        for (int i = next++; i < total; i = next++) {
            if (cancel && *cancel)
                break;
            PagePairResult &result = resultData[i];
            PdfPage page1;
            PdfPage page2;
            if (doc1)
                page1 = doc1->page(pages1.at(i));
            if (doc2)
                page2 = doc2->page(pages2.at(i));
            if (!page1)
                result.unreadableFile = 1;
            else if (!page2)
                result.unreadableFile = 2;
            else
                result.difference = comparePagePair(page1, page2, options);
            result.compared = true;
            ++done;
        }
    };

    // The first time Poppler processes a page it creates some global colour
    // profiles, without locking (GfxState::sRGBProfile and
    // GfxXYZ2DisplayTransforms::XYZProfile).  Workers doing that at once
    // can free each other's profile and crash, so process one page here
    // before they start.
    if (total > 0) {
        const PdfPage page = pdf1->page(pages1.at(0));
        if (page)
            page->textList();
    }

    int wanted = QThread::idealThreadCount();
    if (options.maxWorkers > 0)
        wanted = qMin(wanted, options.maxWorkers);
    const int workers = qBound(1, wanted, qMax(total, 1));
    QList<QFuture<void>> futures;
    // The workers write through references to results, next and done, so
    // none of them may still be running once this function returns.  The
    // explicit wait at the end does that on the normal path and reports
    // the first exception, but it is skipped when a worker or progress()
    // throws, so the wait has to happen while unwinding as well.
    struct WaitForAll
    {
        QList<QFuture<void>> &futures;
        ~WaitForAll()
        {
            for (QFuture<void> &future : futures) {
                try {
                    future.waitForFinished();
                } catch (...) {
                    // Either we are already unwinding, or the loop below
                    // has reported this one: there is nothing to do but
                    // let every worker stop.
                }
            }
        }
    } waitForAll{futures};
    for (int i = 0; i < workers; ++i)
        futures.append(QtConcurrent::run(worker));
    if (progress) {
        auto finished = [&futures]() {
            for (const QFuture<void> &future : futures)
                if (!future.isFinished())
                    return false;
            return true;
        };
        while (!finished()) {
            progress(done);
            QThread::msleep(50);
        }
        progress(done);
    }
    for (QFuture<void> &future : futures)
        future.waitForFinished();
    return results;
}
