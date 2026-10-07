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
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <QFuture>
#include <QImage>
#include <QMultiHash>
#include <QSet>
#include <QThread>
#include <QtConcurrent>


// Fills in what *fingerprint lacks: the words if it is not yet readable,
// and the image hash if withImageHash and it has none.
static void fingerprintPage(const PdfPage &page,
                            const PageCompareOptions &options,
                            const bool withImageHash,
                            PageFingerprint *fingerprint)
{
    const QSize size = page->pageSize();
    if (!fingerprint->readable) {
        QRectF rect;
        if (options.excludeMargins)
            rect = rectForMargins(size.width(), size.height(),
                    options.topMargin, options.bottomMargin,
                    options.leftMargin, options.rightMargin);
        for (const PdfTextBox &box : getTextBoxes(page, rect))
            fingerprint->words.append({box->text(), box->boundingBox()});
        fingerprint->readable = true;
    }
    if (withImageHash && !fingerprint->hasImageHash) {
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
        const QImage image = page->renderToImage(DPI, DPI, x, y, width,
                                                 height);
        QCryptographicHash hash(QCryptographicHash::Sha1);
        hash.addData(QByteArray::number(static_cast<int>(image.format())));
        for (int row = 0; row < image.height(); ++row)
            hash.addData(QByteArrayView(
                    reinterpret_cast<const char*>(image.constScanLine(row)),
                    image.width() * image.depth() / 8));
        fingerprint->imageSize = image.size();
        fingerprint->imageHash = hash.result();
        fingerprint->hasImageHash = true;
    }
}


// True if each word in one list matches a word with the same text in the
// other whose bounding box is within tolerance points of its own.
// Poppler's reading order can change when text moves by a tiny fraction
// of a point (e.g., after rewriting a PDF with Ghostscript), so the same
// words in the same places can come out in a different order.
static bool sameWordsInSamePlaces(const QVector<PageWord> &words1,
                                  const QVector<PageWord> &words2,
                                  const qreal tolerance)
{
    if (words1.count() != words2.count())
        return false;
    QMultiHash<QString, int> index;
    for (int i = 0; i < words2.count(); ++i)
        index.insert(words2[i].text, i);
    QVector<bool> used(words2.count(), false);
    for (const PageWord &word : words1) {
        bool found = false;
        for (auto it = index.find(word.text);
             it != index.end() && it.key() == word.text; ++it) {
            const QRectF &other = words2[it.value()].rect;
            if (!used[it.value()] &&
                qAbs(other.left() - word.rect.left()) <= tolerance &&
                qAbs(other.top() - word.rect.top()) <= tolerance &&
                qAbs(other.right() - word.rect.right()) <= tolerance &&
                qAbs(other.bottom() - word.rect.bottom()) <= tolerance) {
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


PageDifference compareFingerprints(const PageFingerprint &fingerprint1,
                                   const PageFingerprint &fingerprint2,
                                   const bool compareAppearance)
{
    const QVector<PageWord> &words1 = fingerprint1.words;
    const QVector<PageWord> &words2 = fingerprint2.words;
    if (words1.count() != words2.count())
        return TextualPageDifference;
    for (int i = 0; i < words1.count(); ++i)
        if (words1[i].text != words2[i].text) {
            // The words may just be in a different order
            const qreal Tolerance = 0.1; // points
            if (!sameWordsInSamePlaces(words1, words2, Tolerance))
                return TextualPageDifference;
            break;
        }
    if (compareAppearance &&
        (fingerprint1.imageSize != fingerprint2.imageSize ||
         fingerprint1.imageHash != fingerprint2.imageHash))
        return VisualPageDifference;
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


// Identifies a file's current contents, so cached fingerprints are not
// used after it changes
static QString documentKey(const QString &filename)
{
    const QFileInfo info(filename);
    return QString("%1:%2:%3").arg(info.canonicalFilePath())
            .arg(info.size())
            .arg(info.lastModified().toMSecsSinceEpoch());
}


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
        const std::function<void(int, int)> &progress,
        PageFingerprintCache *cache)
{
    const int total = qMin(pages1.count(), pages2.count());
    PageFingerprintCache localCache;
    PageFingerprintCache &fingerprints = cache ? *cache : localCache;

    // Forget the fingerprints of other files
    const QString document1 = documentKey(filename1);
    const QString document2 = documentKey(filename2);
    for (auto it = fingerprints.begin(); it != fingerprints.end(); ) {
        if (it.key().startsWith(document1 + '\n') ||
            it.key().startsWith(document2 + '\n'))
            ++it;
        else
            it = fingerprints.erase(it);
    }
    const QString margins = !options.excludeMargins ? QString()
            : QString("%1:%2:%3:%4").arg(options.topMargin)
                .arg(options.bottomMargin).arg(options.leftMargin)
                .arg(options.rightMargin);
    auto keyFor = [&](const QString &document, const int page) {
        return QString("%1\n%2\n%3").arg(document).arg(page).arg(margins);
    };

    // Find the pages whose fingerprints are missing or, for an Appearance
    // comparison, lack an image hash
    struct Job { int which; int page; QString key; };
    QVector<Job> jobs;
    QSet<QString> queued;
    auto need = [&](const int which, const int page) {
        const QString key = keyFor(which == 1 ? document1 : document2,
                                   page);
        if (queued.contains(key))
            return;
        const auto it = fingerprints.constFind(key);
        if (it == fingerprints.constEnd() ||
            (options.compareAppearance && it->readable &&
             !it->hasImageHash)) {
            jobs.append({which, page, key});
            queued.insert(key);
        }
    };
    for (int i = 0; i < total; ++i) {
        need(1, pages1.at(i));
        need(2, pages2.at(i));
    }

    const int jobCount = jobs.count();
    QVector<PageFingerprint> results(jobCount);
    QVector<bool> finished(jobCount, false);
    for (int j = 0; j < jobCount; ++j)
        results[j] = fingerprints.value(jobs.at(j).key);
    // Workers write through these rather than QVector::operator[], which
    // may detach
    PageFingerprint *const resultData = results.data();
    bool *const finishedData = finished.data();
    std::atomic<int> next(0);
    std::atomic<int> done(0);

    auto worker = [&]() {
        const PdfDocument doc1 = loadCopy(filename1, pdf1);
        const PdfDocument doc2 = loadCopy(filename2, pdf2);
        for (int j = next++; j < jobCount; j = next++) {
            if (cancel && *cancel)
                break;
            const Job &job = jobs.at(j);
            const PdfDocument &doc = job.which == 1 ? doc1 : doc2;
            PdfPage page;
            if (doc)
                page = doc->page(job.page);
            if (page)
                fingerprintPage(page, options, options.compareAppearance,
                                &resultData[j]);
            finishedData[j] = true;
            ++done;
        }
    };

    if (jobCount > 0) {
        // The first time Poppler processes a page it creates some global
        // colour profiles, without locking (GfxState::sRGBProfile and
        // GfxXYZ2DisplayTransforms::XYZProfile).  Workers doing that at
        // once can free each other's profile and crash, so process one
        // page here before they start.
        const PdfPage page = pdf1->page(pages1.at(0));
        if (page)
            page->textList();

        int wanted = QThread::idealThreadCount();
        if (options.maxWorkers > 0)
            wanted = qMin(wanted, options.maxWorkers);
        const int workers = qBound(1, wanted, jobCount);
        QList<QFuture<void>> futures;
        // The workers write through references to locals, so none of them
        // may still be running once this function returns.  The explicit
        // wait below does that on the normal path and reports the first
        // exception, but it is skipped when a worker or progress() throws,
        // so the wait has to happen while unwinding as well.
        struct WaitForAll
        {
            QList<QFuture<void>> &futures;
            ~WaitForAll()
            {
                for (QFuture<void> &future : futures) {
                    try {
                        future.waitForFinished();
                    } catch (...) {
                        // Either we are already unwinding, or the loop
                        // below has reported this one: there is nothing
                        // to do but let every worker stop.
                    }
                }
            }
        } waitForAll{futures};
        for (int i = 0; i < workers; ++i)
            futures.append(QtConcurrent::run(worker));
        if (progress) {
            auto allFinished = [&futures]() {
                for (const QFuture<void> &future : futures)
                    if (!future.isFinished())
                        return false;
                return true;
            };
            while (!allFinished()) {
                progress(done, jobCount);
                QThread::msleep(50);
            }
            progress(done, jobCount);
        }
        for (QFuture<void> &future : futures)
            future.waitForFinished();
        for (int j = 0; j < jobCount; ++j)
            if (finished.at(j))
                fingerprints.insert(jobs.at(j).key, results.at(j));
    }

    QVector<PagePairResult> pairResults(total);
    for (int i = 0; i < total; ++i) {
        const auto it1 = fingerprints.constFind(keyFor(document1,
                                                        pages1.at(i)));
        const auto it2 = fingerprints.constFind(keyFor(document2,
                                                        pages2.at(i)));
        if (it1 == fingerprints.constEnd() || it2 == fingerprints.constEnd())
            continue; // Cancelled before getting to these pages
        PagePairResult &result = pairResults[i];
        if (!it1->readable)
            result.unreadableFile = 1;
        else if (!it2->readable)
            result.unreadableFile = 2;
        else
            result.difference = compareFingerprints(*it1, *it2,
                    options.compareAppearance);
        result.compared = true;
    }
    return pairResults;
}
