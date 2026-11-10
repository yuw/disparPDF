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
#include <QThread>
#include <QtConcurrent>


// The rendering that an Appearance comparison looks at: the page at
// 72 DPI, cropped to the margins if they are being excluded
static QImage renderForComparison(const PdfPage &page,
                                  const PageCompareOptions &options)
{
    const QSize size = page->pageSize();
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
    return page->renderToImage(DPI, DPI, x, y, width, height);
}


// Fills in what *fingerprint lacks: the words if it is not yet readable,
// and the image hash if withImageHash and it has none.
static void fingerprintPage(const PdfPage &page,
                            const PageCompareOptions &options,
                            const bool withImageHash,
                            PageFingerprint *fingerprint)
{
    if (!fingerprint->readable) {
        const QSize size = page->pageSize();
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
        const QImage image = renderForComparison(page, options);
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


bool hasUnmappedCharacter(const QString &text)
{
    for (const QChar c : text) {
        if (c.isSpace())
            continue;
        if (c == QChar::ReplacementCharacter)
            return true;
        switch (c.category()) {
        case QChar::Other_Control:
        case QChar::Other_PrivateUse:
        case QChar::Other_NotAssigned:
            return true;
        default:
            break;
        }
    }
    return false;
}


QVector<QPair<int, int>> sameLookingUnmappedTexts(
        const QStringList &texts1, const QList<QRectF> &rects1,
        const QStringList &texts2, const QList<QRectF> &rects2,
        const PdfPage &page1, const PdfPage &page2)
{
    QVector<QPair<int, int>> same;
    if (!page1 || !page2)
        return same;
    // Two fonts may give the same glyph slightly different boxes, and
    // what decides is the rendering, so this can be generous
    const qreal Tolerance = 1.0; // points
    auto inSamePlace = [&](const QRectF &a, const QRectF &b) {
        return qAbs(a.left() - b.left()) <= Tolerance &&
               qAbs(a.top() - b.top()) <= Tolerance &&
               qAbs(a.right() - b.right()) <= Tolerance &&
               qAbs(a.bottom() - b.bottom()) <= Tolerance;
    };
    QVector<bool> used1(texts1.count(), false);
    QVector<bool> used2(texts2.count(), false);
    QImage image1;
    QImage image2;
    auto looksSame = [&](const QRectF &rect1, const QRectF &rect2) {
        if (image1.isNull()) {
            const int DPI = POINTS_PER_INCH; // so a point is a pixel
            image1 = page1->renderToImage(DPI, DPI);
            image2 = page2->renderToImage(DPI, DPI);
        }
        // A pixel more all round, for antialiasing beyond the box
        const QRect rect = rect1.united(rect2).toAlignedRect()
                .adjusted(-1, -1, 1, 1)
                .intersected(image1.rect()).intersected(image2.rect());
        return !rect.isEmpty() && image1.copy(rect) == image2.copy(rect);
    };
    // Tries text i of the first page (or, if swapped, of the second)
    // against whatever is in its place on the other
    auto pairUp = [&](const int i, const bool swapped) {
        const QStringList &texts = swapped ? texts2 : texts1;
        const QStringList &others = swapped ? texts1 : texts2;
        const QList<QRectF> &rects = swapped ? rects2 : rects1;
        const QList<QRectF> &otherRects = swapped ? rects1 : rects2;
        QVector<bool> &used = swapped ? used2 : used1;
        QVector<bool> &otherUsed = swapped ? used1 : used2;
        if (used[i] || !hasUnmappedCharacter(texts.at(i)))
            return;
        for (int j = 0; j < others.count(); ++j) {
            if (otherUsed[j] ||
                !inSamePlace(rects.at(i), otherRects.at(j)))
                continue;
            if (texts.at(i) != others.at(j) &&
                looksSame(rects.at(i), otherRects.at(j))) {
                used[i] = otherUsed[j] = true;
                same.append(swapped ? qMakePair(j, i) : qMakePair(i, j));
            }
            // One text to a place, whether or not it looked the same
            return;
        }
    };
    for (int i = 0; i < texts1.count(); ++i)
        pairUp(i, false);
    for (int j = 0; j < texts2.count(); ++j)
        pairUp(j, true);
    return same;
}


static bool anyUnmapped(const QVector<PageWord> &words)
{
    for (const PageWord &word : words)
        if (hasUnmappedCharacter(word.text))
            return true;
    return false;
}


// Gives the words of words2 that sameLookingUnmappedTexts() pairs with
// words of words1 those words' texts; false if there are none
static bool reconcileUnmappedWords(const QVector<PageWord> &words1,
                                   QVector<PageWord> *words2,
                                   const PdfPage &page1,
                                   const PdfPage &page2)
{
    QStringList texts1;
    QStringList texts2;
    QList<QRectF> rects1;
    QList<QRectF> rects2;
    for (const PageWord &word : words1) {
        texts1 << word.text;
        rects1 << word.rect;
    }
    for (const PageWord &word : std::as_const(*words2)) {
        texts2 << word.text;
        rects2 << word.rect;
    }
    const QVector<QPair<int, int>> same = sameLookingUnmappedTexts(
            texts1, rects1, texts2, rects2, page1, page2);
    for (const QPair<int, int> &pair : same)
        (*words2)[pair.second].text = words1.at(pair.first).text;
    return !same.isEmpty();
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


QString documentKey(const QString &filename)
{
    const QFileInfo info(filename);
    return QString("%1:%2:%3").arg(info.canonicalFilePath())
            .arg(info.size())
            .arg(info.lastModified().toMSecsSinceEpoch());
}


// Another copy of model, from the file or from data if it is not empty
static PdfDocument loadCopy(const QString &filename, const QByteArray &data,
                            const PdfDocument &model)
{
    PdfDocument pdf(data.isEmpty() ? Poppler::Document::load(filename)
                                   : Poppler::Document::loadFromData(data));
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
    // An image hash only earns its cost when something will ask about the
    // same rendering again, which needs a cache to keep it in.  Without
    // one -- the batch path -- a pair's two renderings are compared
    // directly and dropped, so neither is hashed or stored.
    const bool hashImages = cache != nullptr;

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
        return QString("%1\n%2\n%3\n%4").arg(document).arg(page)
                .arg(margins, options.boxKey);
    };

    // Each pair is one job: a worker opens both of its pages, reads
    // their words and compares those, and renders the pages only if the
    // words match, since a pair that already differs textually needs no
    // rendering at all.  Taking a whole pair at once also means each page
    // is opened once for both its words and its rendering.
    struct PairJob { int page1; int page2; QString key1; QString key2; };
    QVector<PairJob> jobs(total);
    for (int i = 0; i < total; ++i)
        jobs[i] = {pages1.at(i), pages2.at(i),
                   keyFor(document1, pages1.at(i)),
                   keyFor(document2, pages2.at(i))};

    QVector<PagePairResult> pairResults(total);
    // What each job ended up with, to be merged into the cache on this
    // thread once every worker has stopped
    struct PairFingerprints { PageFingerprint fingerprint1, fingerprint2; };
    QVector<PairFingerprints> computed(total);
    // Workers write through these rather than QVector::operator[], which
    // may detach
    PagePairResult *const resultData = pairResults.data();
    PairFingerprints *const computedData = computed.data();
    std::atomic<int> next(0);
    std::atomic<int> done(0);

    auto worker = [&]() {
        const PdfDocument doc1 = loadCopy(filename1, options.boxedDocument1,
                                          pdf1);
        const PdfDocument doc2 = loadCopy(filename2, options.boxedDocument2,
                                          pdf2);
        for (int j = next++; j < total; j = next++) {
            if (cancel && *cancel)
                break;
            const PairJob &job = jobs.at(j);
            // Reading the cache here is safe: nothing writes to it until
            // every worker has stopped
            PageFingerprint fingerprint1 = fingerprints.value(job.key1);
            PageFingerprint fingerprint2 = fingerprints.value(job.key2);
            PdfPage page1;
            PdfPage page2;
            if (!fingerprint1.readable && doc1)
                page1 = doc1->page(job.page1);
            if (!fingerprint2.readable && doc2)
                page2 = doc2->page(job.page2);
            if (page1)
                fingerprintPage(page1, options, false, &fingerprint1);
            if (page2)
                fingerprintPage(page2, options, false, &fingerprint2);

            PagePairResult &result = resultData[j];
            if (!fingerprint1.readable) {
                result.unreadableFile = 1;
            } else if (!fingerprint2.readable) {
                result.unreadableFile = 2;
            } else {
                result.difference = compareFingerprints(fingerprint1,
                        fingerprint2, false);
                // Words that differ may be ones Poppler could not read
                // (see hasUnmappedCharacter()); if those that are left
                // differing all look the same, the pages do not differ
                // textually after all.  The fingerprints keep the words
                // as they were read: which of them look the same depends
                // on the other page of the pair.
                if (result.difference == TextualPageDifference &&
                    (anyUnmapped(fingerprint1.words) ||
                     anyUnmapped(fingerprint2.words))) {
                    if (!page1 && doc1)
                        page1 = doc1->page(job.page1);
                    if (!page2 && doc2)
                        page2 = doc2->page(job.page2);
                    PageFingerprint seen2;
                    seen2.readable = true;
                    seen2.words = fingerprint2.words;
                    if (page1 && page2 &&
                        reconcileUnmappedWords(fingerprint1.words,
                                &seen2.words, page1, page2))
                        result.difference = compareFingerprints(
                                fingerprint1, seen2, false);
                }
                // Only now is rendering worth it: the words match, so the
                // pages can still differ in appearance
                if (result.difference == NoPageDifference &&
                    options.compareAppearance) {
                    if (!hashImages) {
                        // Nothing will ask about these renderings again,
                        // so compare them and let them go.  QImage's
                        // comparison stops at the first differing byte,
                        // where hashing has to read all of both.
                        QImage image1;
                        QImage image2;
                        if (page1)
                            image1 = renderForComparison(page1, options);
                        if (page2)
                            image2 = renderForComparison(page2, options);
                        if (image1 != image2)
                            result.difference = VisualPageDifference;
                    } else {
                        if (!fingerprint1.hasImageHash) {
                            if (!page1 && doc1)
                                page1 = doc1->page(job.page1);
                            if (page1)
                                fingerprintPage(page1, options, true,
                                                &fingerprint1);
                        }
                        if (!fingerprint2.hasImageHash) {
                            if (!page2 && doc2)
                                page2 = doc2->page(job.page2);
                            if (page2)
                                fingerprintPage(page2, options, true,
                                                &fingerprint2);
                        }
                        // Not compareFingerprints(): the words have
                        // been compared, and may have matched only after
                        // looking at ones that could not be read
                        if (fingerprint1.imageSize != fingerprint2.imageSize
                            || fingerprint1.imageHash !=
                               fingerprint2.imageHash)
                            result.difference = VisualPageDifference;
                    }
                }
            }
            result.compared = true;
            if (hashImages)
                computedData[j] = {fingerprint1, fingerprint2};
            ++done;
        }
    };

    if (total > 0) {
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
        const int workers = qBound(1, wanted, total);
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
                progress(done, total);
                QThread::msleep(50);
            }
            progress(done, total);
        }
        for (QFuture<void> &future : futures)
            future.waitForFinished();

        // Keep whichever fingerprint says more: a page in two pairs may
        // have been rendered for one of them and not for the other.
        // Without a cache there is nothing to keep them for.
        if (hashImages) {
            auto keep = [&](const QString &key, const PageFingerprint &f) {
                if (!f.readable)
                    return;
                const auto it = fingerprints.constFind(key);
                if (it != fingerprints.constEnd() && it->hasImageHash &&
                    !f.hasImageHash)
                    return;
                fingerprints.insert(key, f);
            };
            for (int j = 0; j < total; ++j) {
                if (!pairResults.at(j).compared)
                    continue;
                keep(jobs.at(j).key1, computed.at(j).fingerprint1);
                keep(jobs.at(j).key2, computed.at(j).fingerprint2);
            }
        }
    }
    return pairResults;
}
