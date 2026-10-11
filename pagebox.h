#ifndef PAGEBOX_H
#define PAGEBOX_H
/*
    Copyright © 2026 Yuwsuke Kieda. All rights reserved.
    This program or module is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 2 of
    the License, or (at your option) any later version. This program is
    distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
*/

#include <QByteArray>
#include <QList>
#include <QRectF>
#include <QString>
#include <QVector>

// Page boundaries (ISO 32000-2:2020, 14.11.2 and Table 31), for comparing
// only what lies inside one of them.
//
// Poppler's Qt6 binding exposes only the crop box, and clips rendering and
// text extraction to it, so the boxes are read with qpdf, which can also
// write a copy of the document whose MediaBox and CropBox are the region
// to compare on every page.  Opened with Poppler, that copy renders and
// extracts exactly the region, with its top-left corner as the origin.

enum class PageBox {None, Media, Crop, Bleed, Trim, Art};

// How the chosen box is clipped:
//   Media  to the media box, as 14.11.2.1 requires of a processor
//   Crop   to the media box and the crop box, i.e. what a viewer shows
//   None   not at all: the box as written
enum class BoxClip {Media, Crop, None};

QString pageBoxName(PageBox box);   // "none", "media" ... "art"
bool parsePageBox(const QString &name, PageBox *box);
QString boxClipName(BoxClip clip);  // "media", "crop", "none"
bool parseBoxClip(const QString &name, BoxClip *clip);

// A rectangle in default user space: lower-left and upper-right corners,
// y growing upward.  Always normalised (llx <= urx, lly <= ury).
struct BoxRect
{
    double llx = 0;
    double lly = 0;
    double urx = 0;
    double ury = 0;

    BoxRect() = default;
    BoxRect(double x1, double y1, double x2, double y2);  // any two corners

    double width() const { return urx - llx; }
    double height() const { return ury - lly; }
    bool isEmpty() const { return width() <= 0 || height() <= 0; }
    BoxRect intersected(const BoxRect &other) const;
    bool contains(const BoxRect &other) const;
    bool fuzzyEquals(const BoxRect &other, double tolerance) const;
};

// The boxes of one page, as written in the file (after inheritance, but
// before defaults or any clipping)
struct PageBoxes
{
    BoxRect media;
    BoxRect crop, bleed, trim, art;
    bool hasCrop = false;
    bool hasBleed = false;
    bool hasTrim = false;
    bool hasArt = false;
    int rotate = 0;  // 0, 90, 180 or 270

    bool has(PageBox box) const;
    // The box as written or, if absent, its default from Table 31: the
    // crop box for the bleed, trim and art boxes, the media box for the
    // crop box
    BoxRect withDefault(PageBox box) const;
    // What Poppler treats as the page: the crop box clipped to the media
    // box
    BoxRect visible() const;
    // A rectangle in default user space, in the coordinates Poppler uses
    // for this page: points from the top-left corner of visible(), after
    // /Rotate
    QRectF toPageCoordinates(const BoxRect &rect) const;
};

// The region to compare on one page, and what is worth a warning
struct BoxRegion
{
    BoxRect rect;              // in default user space
    bool absent = false;       // the box is not in the page: its default
                               // was used
    bool beyondCrop = false;   // Crop: the box extends past the crop box
    bool beyondMedia = false;  // None: the box extends past the media box
};

BoxRegion boxRegion(const PageBoxes &boxes, PageBox box, BoxClip clip);

// What is said about one kind of warning, for one file or for the pair
struct BoxWarning
{
    QString kind;   // "absent", "beyond-crop", "beyond-media", "empty",
                    // "differ" or "clip-without-box"
    int file = 0;   // 1 or 2, or 0 for both
    QString pages;  // e.g. "1-3, 5" (pages of FILE1, for "differ")
    QString message;
};

// The warnings for comparing pages1 of the first file with pages2 of the
// second (0-based, as many of each as are compared), each kind reported
// once per file with the pages it applies to
QVector<BoxWarning> boxWarnings(const QVector<PageBoxes> &boxes1,
                                const QVector<BoxRegion> &regions1,
                                const QList<int> &pages1,
                                const QVector<PageBoxes> &boxes2,
                                const QVector<BoxRegion> &regions2,
                                const QList<int> &pages2,
                                PageBox box);

// A PDF's page boxes, and copies of it rewritten to a region per page
class BoxedDocument
{
public:
    // Reads filename's boxes.  On failure returns false and sets *error.
    bool load(const QString &filename, QString *error);

    int pageCount() const { return m_boxes.size(); }
    const PageBoxes &boxes(int page) const { return m_boxes.at(page); }

    // A copy of the document in which every page's MediaBox and CropBox
    // are regions[page].rect; empty on failure, with *error set.  A page
    // whose region is empty becomes a blank 1pt square, since Poppler
    // replaces a MediaBox without an area by a default one.
    QByteArray rewritten(const QVector<BoxRegion> &regions,
                         QString *error) const;

    // boxRegion() for every page
    QVector<BoxRegion> regions(PageBox box, BoxClip clip) const;
    const QVector<PageBoxes> &allBoxes() const { return m_boxes; }

private:
    QByteArray m_data;  // the file, so rewriting does not read it again
    QVector<PageBoxes> m_boxes;
};

#endif // PAGEBOX_H
