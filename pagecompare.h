#ifndef PAGECOMPARE_H
#define PAGECOMPARE_H
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

#include "generic.hpp"
#include <QList>
#include <QString>
#include <QVector>
#include <atomic>
#include <functional>

// Comparison of page pairs, shared by MainWindow and BatchCompare.

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
};

struct PagePairResult
{
    bool compared = false;   // false if skipped because of cancellation
    int unreadableFile = 0;  // 1 or 2 if that file's page could not be read
    PageDifference difference = NoPageDifference;
};

PageDifference comparePagePair(const PdfPage &page1, const PdfPage &page2,
                               const PageCompareOptions &options);

// Compares page pages1[i] of pdf1 with page pages2[i] of pdf2, for each i,
// using one worker thread per core.  A Poppler document must not be
// rendered from several threads at once, so each worker loads its own
// copies of the two files (with the same render hints as pdf1 and pdf2).
// Results are returned in page order.  If progress is set it is called on
// the calling thread, with the number of pairs compared so far, every
// 50 ms or so until the comparison is complete; it may process events.
// Setting *cancel stops the comparison early.
QVector<PagePairResult> comparePagesInParallel(
        const QString &filename1, const PdfDocument &pdf1,
        const QList<int> &pages1,
        const QString &filename2, const PdfDocument &pdf2,
        const QList<int> &pages2,
        const PageCompareOptions &options,
        const std::atomic<bool> *cancel = nullptr,
        const std::function<void(int)> &progress = nullptr);

#endif // PAGECOMPARE_H
