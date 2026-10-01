/*
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
#include <QThread>
#include <QtConcurrent>


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
        if (list1[i]->text() != list2[i]->text())
            return TextualPageDifference;

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


static PdfDocument loadCopy(const QString &filename, const PdfDocument &model)
{
    PdfDocument pdf(Poppler::Document::load(filename));
    if (!pdf || pdf->isLocked())
        return PdfDocument();
    for (int bit = Poppler::Document::Antialiasing;
         bit <= Poppler::Document::HideAnnotations; bit <<= 1) {
        const auto hint = static_cast<Poppler::Document::RenderHint>(bit);
        pdf->setRenderHint(hint, model->renderHints().testFlag(hint));
    }
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

    const int workers = qBound(1, QThread::idealThreadCount(), qMax(total, 1));
    QList<QFuture<void>> futures;
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
