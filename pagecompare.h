#ifndef PAGECOMPARE_H
#define PAGECOMPARE_H
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

#include "generic.hpp"
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>
#include <atomic>
#include <functional>

// Comparison of page pairs, shared by MainWindow and BatchCompare.
//
// Deciding whether two pages differ needs only a few facts about each
// page: its words and where they are and, when comparing appearance, its
// rendering.  These are computed once per page as a "fingerprint", which
// can be kept in a PageFingerprintCache, so that a page can be compared
// again, or with any other page, without asking Poppler again.

enum PageDifference {NoPageDifference, TextualPageDifference,
                     VisualPageDifference};

struct PageCompareOptions
{
    bool compareAppearance = false;
    bool excludeMargins = false;
    int topMargin = 0;
    int bottomMargin = 0;
    int leftMargin = 0;
    int rightMargin = 0;
    // Upper bound on worker threads; 0 means one per core.  Each worker
    // holds its own pair of open documents, so peak memory grows with
    // this number: lower it on machines with many cores and little RAM.
    int maxWorkers = 0;
};

struct PagePairResult
{
    bool compared = false;   // false if skipped because of cancellation
    int unreadableFile = 0;  // 1 or 2 if that file's page could not be read
    PageDifference difference = NoPageDifference;
};

struct PageWord
{
    QString text;
    QRectF rect;
};

struct PageFingerprint
{
    bool readable = false;      // false if Poppler could not read the page
    QVector<PageWord> words;    // inside the margins, in Poppler's order
    bool hasImageHash = false;  // only computed for Appearance comparisons
    QSize imageSize;
    QByteArray imageHash;       // of the 72 DPI rendering inside the margins
};

// Keyed by file (path, size and modification time), page, and margins
typedef QHash<QString, PageFingerprint> PageFingerprintCache;

PageDifference compareFingerprints(const PageFingerprint &fingerprint1,
                                   const PageFingerprint &fingerprint2,
                                   const bool compareAppearance);

// Compares page pages1[i] of pdf1 with page pages2[i] of pdf2, for each i,
// and returns the results in that order.  The pairs are shared out among
// one worker thread per core, or options.maxWorkers if that is set; a
// page is not read again if cache already has its fingerprint, and is
// rendered only if its pair's words match.  A Poppler document must not
// be rendered from several threads at once, so each worker loads its own
// copies of the two files (with the same render hints as pdf1 and pdf2).
// If cache is given, fingerprints of other files are removed from it and
// new ones added once the workers have stopped; until then the workers
// read it, so nothing else may change it while this runs.  If progress is
// set it is called on the calling thread, with the number of pairs
// compared so far and the number to do, every 50 ms or so until done; it
// may process events.  Setting *cancel stops the comparison early.
QVector<PagePairResult> comparePagesInParallel(
        const QString &filename1, const PdfDocument &pdf1,
        const QList<int> &pages1,
        const QString &filename2, const PdfDocument &pdf2,
        const QList<int> &pages2,
        const PageCompareOptions &options,
        const std::atomic<bool> *cancel = nullptr,
        const std::function<void(int, int)> &progress = nullptr,
        PageFingerprintCache *cache = nullptr);

#endif // PAGECOMPARE_H
