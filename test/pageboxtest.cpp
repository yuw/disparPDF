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

// Unit tests of pagebox.cpp, on data/boxes.pdf (see
// data/make-test-pdfs.sh for its pages):
//
//   pageboxtest BOXES.PDF
//
// checks the boxes read from it, the regions computed from them, that
// toPageCoordinates() agrees with where Poppler puts words on rotated
// pages, and what a rewritten copy contains.

#include "../pagebox.h"
#include <poppler-qt6.h>
#include <QCoreApplication>
#include <QTextStream>
#include <cmath>
#include <memory>

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    if (!ok) {
        out << "FAIL: " << what << "\n";
        ++failures;
    }
}

static QString str(const BoxRect &r)
{
    return QString("[%1 %2 %3 %4]").arg(r.llx).arg(r.lly).arg(r.urx)
                                   .arg(r.ury);
}

static QString str(const QRectF &r)
{
    return QString("(%1,%2 %3x%4)").arg(r.x()).arg(r.y()).arg(r.width())
                                   .arg(r.height());
}

static void checkRect(const BoxRect &got, const BoxRect &expected,
                      const QString &what)
{
    check(got.fuzzyEquals(expected, 0.001),
          QString("%1: %2, expected %3").arg(what, str(got), str(expected)));
}

static bool near(const QRectF &a, const QRectF &b, double tolerance)
{
    return std::abs(a.left() - b.left()) <= tolerance &&
           std::abs(a.top() - b.top()) <= tolerance &&
           std::abs(a.right() - b.right()) <= tolerance &&
           std::abs(a.bottom() - b.bottom()) <= tolerance;
}

// The bounding box Poppler gives the first word containing text, or a
// null rectangle; *word is set to that word
static QRectF wordBox(Poppler::Page *page, const QString &text,
                      QString *word = nullptr)
{
    for (const auto &box : page->textList())
        if (box->text().contains(text)) {
            if (word)
                *word = box->text();
            return box->boundingBox();
        }
    return QRectF();
}

// Where a word on a page with no /Rotate is in default user space
static BoxRect userRect(const PageBoxes &boxes, const QRectF &onPage)
{
    const BoxRect v = boxes.visible();
    return BoxRect(onPage.left() + v.llx, v.ury - onPage.bottom(),
                   onPage.right() + v.llx, v.ury - onPage.top());
}

static void testNames()
{
    PageBox box = PageBox::None;
    check(parsePageBox("Trim", &box) && box == PageBox::Trim,
          "parsePageBox(Trim)");
    check(!parsePageBox("page", &box), "parsePageBox(page) fails");
    BoxClip clip = BoxClip::Media;
    check(parseBoxClip("none", &clip) && clip == BoxClip::None,
          "parseBoxClip(none)");
    check(!parseBoxClip("trim", &clip), "parseBoxClip(trim) fails");
    check(pageBoxName(PageBox::Art) == "art", "pageBoxName(Art)");
    check(BoxRect(10, 20, 0, 5).fuzzyEquals(BoxRect(0, 5, 10, 20), 0),
          "BoxRect normalises its corners");
}

static void testBoxes(const BoxedDocument &doc)
{
    check(doc.pageCount() == 6, QString("%1 pages").arg(doc.pageCount()));
    if (doc.pageCount() != 6)
        return;
    const int rotations[] = {0, 90, 180, 270, 0, 0};
    for (int i = 0; i < 6; ++i)
        check(doc.boxes(i).rotate == rotations[i],
              QString("page %1 rotated by %2").arg(i + 1)
                      .arg(doc.boxes(i).rotate));

    const PageBoxes &p1 = doc.boxes(0);
    check(p1.hasCrop && p1.hasBleed && p1.hasTrim && p1.hasArt,
          "page 1 has every box");
    checkRect(p1.media, BoxRect(0, 0, 612, 792), "page 1 media");
    checkRect(p1.crop, BoxRect(18, 18, 594, 774), "page 1 crop");
    checkRect(p1.bleed, BoxRect(27, 27, 585, 765), "page 1 bleed");
    checkRect(p1.trim, BoxRect(36, 36, 576, 756), "page 1 trim");
    checkRect(p1.art, BoxRect(72, 72, 540, 720), "page 1 art");

    // No boxes but a media box, which does not start at 0 0: every box
    // defaults to it (Table 31)
    const PageBoxes &p5 = doc.boxes(4);
    check(!p5.hasCrop && !p5.hasBleed && !p5.hasTrim && !p5.hasArt,
          "page 5 has only a media box");
    const BoxRect media5(-100, -50, 512, 742);
    checkRect(p5.media, media5, "page 5 media");
    for (PageBox box : {PageBox::Crop, PageBox::Bleed, PageBox::Trim,
                        PageBox::Art})
        checkRect(p5.withDefault(box), media5,
                  "page 5 default " + pageBoxName(box));

    // Boxes outside the crop box and the media box are kept as written
    const PageBoxes &p6 = doc.boxes(5);
    checkRect(p6.bleed, BoxRect(18, 18, 594, 774), "page 6 bleed");
    checkRect(p6.trim, BoxRect(-20, 36, 576, 756), "page 6 trim");
    check(!p6.hasArt, "page 6 has no art box");
    checkRect(p6.withDefault(PageBox::Art), BoxRect(36, 36, 576, 756),
              "page 6 art defaults to the crop box");
}

static void testRegions(const BoxedDocument &doc)
{
    if (doc.pageCount() != 6)
        return;
    const PageBoxes &p1 = doc.boxes(0);
    for (BoxClip clip : {BoxClip::Media, BoxClip::Crop, BoxClip::None}) {
        BoxRegion r = boxRegion(p1, PageBox::Trim, clip);
        checkRect(r.rect, BoxRect(36, 36, 576, 756),
                  "page 1 trim, clip " + boxClipName(clip));
        check(!r.absent && !r.beyondCrop && !r.beyondMedia,
              "page 1 trim, clip " + boxClipName(clip) + ": no warnings");
    }
    // The media box clipped to the crop box is the crop box
    checkRect(boxRegion(p1, PageBox::Media, BoxClip::Crop).rect,
              BoxRect(18, 18, 594, 774), "page 1 media, clip crop");
    check(boxRegion(p1, PageBox::Media, BoxClip::Crop).beyondCrop,
          "page 1 media, clip crop: beyond the crop box");

    BoxRegion absent = boxRegion(doc.boxes(4), PageBox::Trim,
                                 BoxClip::Media);
    check(absent.absent, "page 5 trim is absent");
    checkRect(absent.rect, BoxRect(-100, -50, 512, 742),
              "page 5 trim defaults to the media box");

    const PageBoxes &p6 = doc.boxes(5);
    BoxRegion bleedMedia = boxRegion(p6, PageBox::Bleed, BoxClip::Media);
    checkRect(bleedMedia.rect, BoxRect(18, 18, 594, 774),
              "page 6 bleed, clip media");
    check(!bleedMedia.beyondCrop, "page 6 bleed, clip media: no warning");
    BoxRegion bleedCrop = boxRegion(p6, PageBox::Bleed, BoxClip::Crop);
    checkRect(bleedCrop.rect, BoxRect(36, 36, 576, 756),
              "page 6 bleed, clip crop");
    check(bleedCrop.beyondCrop, "page 6 bleed, clip crop: beyond the crop");
    BoxRegion trimMedia = boxRegion(p6, PageBox::Trim, BoxClip::Media);
    checkRect(trimMedia.rect, BoxRect(0, 36, 576, 756),
              "page 6 trim, clip media (14.11.2.1)");
    check(!trimMedia.beyondMedia, "page 6 trim, clip media: no warning");
    BoxRegion trimNone = boxRegion(p6, PageBox::Trim, BoxClip::None);
    checkRect(trimNone.rect, BoxRect(-20, 36, 576, 756),
              "page 6 trim, clip none");
    check(trimNone.beyondMedia, "page 6 trim, clip none: beyond the media");
}

// Pages 1-4 differ only in /Rotate, so MARK is at the same place in user
// space on each: where page 1 shows it
static void testPageCoordinates(const BoxedDocument &doc,
                                Poppler::Document *pdf)
{
    if (doc.pageCount() != 6)
        return;
    std::unique_ptr<Poppler::Page> page1 = pdf->page(0);
    const QRectF onPage1 = wordBox(page1.get(), "MARK");
    check(!onPage1.isNull(), "MARK on page 1");
    const BoxRect mark = userRect(doc.boxes(0), onPage1);
    for (int i = 0; i < 4; ++i) {
        const PageBoxes &boxes = doc.boxes(i);
        std::unique_ptr<Poppler::Page> page = pdf->page(i);
        const QRectF expected = boxes.toPageCoordinates(mark);
        const QRectF got = wordBox(page.get(), "MARK");
        check(near(got, expected, 0.5),
              QString("page %1 MARK at %2, expected %3").arg(i + 1)
                      .arg(str(got), str(expected)));
        const QRectF pageRect = boxes.toPageCoordinates(boxes.visible());
        check(std::abs(pageRect.width() - page->pageSizeF().width()) < 0.01 &&
              std::abs(pageRect.height() - page->pageSizeF().height()) < 0.01 &&
              std::abs(pageRect.x()) < 0.01 && std::abs(pageRect.y()) < 0.01,
              QString("page %1 is %2").arg(i + 1).arg(str(pageRect)));
    }
}

static std::unique_ptr<Poppler::Document> rewrite(const BoxedDocument &doc,
                                                  PageBox box, BoxClip clip,
                                                  QVector<BoxRegion> *regions)
{
    regions->clear();
    for (int i = 0; i < doc.pageCount(); ++i)
        regions->append(boxRegion(doc.boxes(i), box, clip));
    QString error;
    const QByteArray data = doc.rewritten(*regions, &error);
    check(!data.isEmpty(), "rewriting: " + error);
    std::unique_ptr<Poppler::Document> pdf =
            Poppler::Document::loadFromData(data);
    check(pdf && !pdf->isLocked(), "opening the rewritten copy");
    return pdf;
}

static void testRewritten(const BoxedDocument &doc, Poppler::Document *pdf)
{
    if (doc.pageCount() != 6)
        return;
    std::unique_ptr<Poppler::Page> page1 = pdf->page(0);
    const BoxRect mark = userRect(doc.boxes(0), wordBox(page1.get(), "MARK"));

    QVector<BoxRegion> regions;
    std::unique_ptr<Poppler::Document> trim =
            rewrite(doc, PageBox::Trim, BoxClip::Media, &regions);
    if (!trim)
        return;
    check(trim->numPages() == 6, "the rewritten copy has 6 pages");
    for (int i = 0; i < 4; ++i) {
        std::unique_ptr<Poppler::Page> page = trim->page(i);
        // Its page is the region, with the same /Rotate
        PageBoxes boxes;
        boxes.media = regions.at(i).rect;
        boxes.rotate = doc.boxes(i).rotate;
        const QSizeF size = boxes.toPageCoordinates(boxes.media).size();
        check(std::abs(page->pageSizeF().width() - size.width()) < 0.01 &&
              std::abs(page->pageSizeF().height() - size.height()) < 0.01,
              QString("trimmed page %1 is %2x%3").arg(i + 1)
                      .arg(page->pageSizeF().width())
                      .arg(page->pageSizeF().height()));
        const QRectF got = wordBox(page.get(), "MARK");
        const QRectF expected = boxes.toPageCoordinates(mark);
        check(near(got, expected, 0.5),
              QString("trimmed page %1 MARK at %2, expected %3").arg(i + 1)
                      .arg(str(got), str(expected)));
        check(!wordBox(page.get(), "ARTGAP").isNull(),
              QString("trimmed page %1 keeps ARTGAP").arg(i + 1));
        check(wordBox(page.get(), "OUTCROP").isNull(),
              QString("trimmed page %1 drops OUTCROP").arg(i + 1));
    }

    // A word straddling the art box keeps only the characters inside it
    std::unique_ptr<Poppler::Document> art =
            rewrite(doc, PageBox::Art, BoxClip::Media, &regions);
    if (art) {
        std::unique_ptr<Poppler::Page> page = art->page(0);
        QString word;
        wordBox(page.get(), "GAP", &word);
        check(word == "GAP", "page 1 in the art box: ARTGAP is " + word);
    }

    // The media box with the crop box: OUTCROP is outside it ...
    std::unique_ptr<Poppler::Document> crop =
            rewrite(doc, PageBox::Media, BoxClip::Crop, &regions);
    if (crop) {
        std::unique_ptr<Poppler::Page> page = crop->page(0);
        check(wordBox(page.get(), "OUTCROP").isNull(),
              "media clipped to crop drops OUTCROP");
    }
    // ... and without it, inside
    std::unique_ptr<Poppler::Document> media =
            rewrite(doc, PageBox::Media, BoxClip::Media, &regions);
    if (media) {
        std::unique_ptr<Poppler::Page> page = media->page(0);
        check(!wordBox(page.get(), "OUTCROP").isNull(),
              "media clipped to media keeps OUTCROP");
    }

    // Page 6's trim box starts outside the media box, and so does NOMEDIA:
    // clipped to the media box only the characters inside are left, and
    // unclipped all of them
    QString word;
    std::unique_ptr<Poppler::Page> trim6 = trim->page(5);
    wordBox(trim6.get(), "MEDIA", &word);
    check(word == "OMEDIA", "page 6 trim clipped to media: NOMEDIA is "
                            + word);
    std::unique_ptr<Poppler::Document> none =
            rewrite(doc, PageBox::Trim, BoxClip::None, &regions);
    if (none) {
        std::unique_ptr<Poppler::Page> page = none->page(5);
        wordBox(page.get(), "MEDIA", &word);
        check(word == "NOMEDIA", "page 6 trim unclipped: NOMEDIA is " + word);
    }
}

// What is shown: the crop box and the region together, and where in that
// the region is
static void testDisplay(const BoxedDocument &doc)
{
    if (doc.pageCount() != 6)
        return;
    struct Case { int page; PageBox box; BoxClip clip; BoxRect area;
                  QRectF boxRect; };
    const Case cases[] = {
        // The trim box inside the crop box: the crop box is shown
        {0, PageBox::Trim, BoxClip::Media, BoxRect(18, 18, 594, 774),
         QRectF(18, 18, 540, 720)},
        // ... and rotated by 90, the box comes 18pt from the top left of
        // the turned page too, and is turned with it
        {1, PageBox::Trim, BoxClip::Media, BoxRect(18, 18, 594, 774),
         QRectF(18, 18, 720, 540)},
        // The media box beyond the crop box: all of it is shown
        {0, PageBox::Media, BoxClip::Media, BoxRect(0, 0, 612, 792),
         QRectF(0, 0, 612, 792)},
        // An unclipped trim box beyond the media box widens what is shown
        {5, PageBox::Trim, BoxClip::None, BoxRect(-20, 36, 576, 756),
         QRectF(0, 0, 596, 720)},
    };
    for (const Case &c : cases) {
        const BoxDisplay d = boxDisplay(doc.boxes(c.page),
                boxRegion(doc.boxes(c.page), c.box, c.clip));
        const QString what = QString("page %1 %2/%3 shown").arg(c.page + 1)
                .arg(pageBoxName(c.box), boxClipName(c.clip));
        checkRect(d.area, c.area, what);
        check(near(d.boxRect, c.boxRect, 0.001),
              QString("%1: the box at %2, expected %3").arg(what)
                      .arg(str(d.boxRect), str(c.boxRect)));
    }
    // An empty region shows the page, with no box on it
    BoxRegion empty;
    const BoxDisplay d = boxDisplay(doc.boxes(0), empty);
    checkRect(d.area, BoxRect(18, 18, 594, 774), "an empty region shown");
    check(d.boxRect.isNull(), "an empty region has no box shown");
}

// A region with nothing left in it makes a blank page, not one of
// Poppler's default size
static void testEmptyRegion(const BoxedDocument &doc)
{
    if (doc.pageCount() != 6)
        return;
    QVector<BoxRegion> regions = doc.regions(PageBox::Trim, BoxClip::Media);
    regions[0].rect = BoxRect(700, 100, 800, 200)
                      .intersected(doc.boxes(0).media);
    check(regions[0].rect.isEmpty(), "outside the media box is empty");
    QString error;
    std::unique_ptr<Poppler::Document> pdf =
            Poppler::Document::loadFromData(doc.rewritten(regions, &error));
    check(pdf != nullptr, "rewriting with an empty region: " + error);
    if (!pdf)
        return;
    std::unique_ptr<Poppler::Page> page = pdf->page(0);
    check(page->pageSizeF() == QSizeF(1, 1),
          QString("the empty page is %1x%2").arg(page->pageSizeF().width())
                  .arg(page->pageSizeF().height()));
    check(page->textList().empty(), "the empty page has no text");
    std::unique_ptr<Poppler::Page> other = pdf->page(1);
    check(!wordBox(other.get(), "MARK").isNull(),
          "the other pages are as before");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        out << "usage: pageboxtest BOXES.PDF\n";
        return 2;
    }
    const QString filename = QString::fromLocal8Bit(argv[1]);
    testNames();
    BoxedDocument doc;
    QString error;
    check(doc.load(filename, &error), "loading: " + error);
    std::unique_ptr<Poppler::Document> pdf =
            Poppler::Document::load(filename);
    check(pdf && !pdf->isLocked(), "opening with Poppler");
    testBoxes(doc);
    testRegions(doc);
    if (pdf) {
        testPageCoordinates(doc, pdf.get());
        testRewritten(doc, pdf.get());
    }
    testDisplay(doc);
    testEmptyRegion(doc);

    BoxedDocument missing;
    check(!missing.load(filename + ".missing", &error),
          "loading a missing file fails");

    if (failures)
        out << failures << " failed\n";
    return failures ? 1 : 0;
}
