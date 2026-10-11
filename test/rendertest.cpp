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

// Tests of what DifferenceRenderer::pixmaps() draws for pages compared
// inside a page box -- the path the views, Save As and --pdfdiff share --
// on the PDFs in data/ (see data/make-test-pdfs.sh):
//
//   rendertest DATA_DIRECTORY
//
// At 100% a pixel is a point, so the positions below are in points from
// the top left of the page shown.

#include "../pagebox.h"
#include "../renderer.h"
#include <QApplication>
#include <QTextStream>

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    if (!ok) {
        out << "FAIL: " << what << "\n";
        ++failures;
    }
}

static bool reddish(QRgb c)
{
    return qRed(c) > qGreen(c) + 12 && qRed(c) > qBlue(c) + 12;
}

static bool bluish(QRgb c)
{
    return qBlue(c) > qRed(c) + 60;
}

// A file compared inside the box, and the views that show it
struct Side
{
    BoxedFile boxed;
    PdfDocument pdf;
    BoxedView view;
};

static bool prepare(const QString &filename, const RenderSettings &settings,
                    Side *side)
{
    bool locked = false;
    PdfDocument whole = loadPdf(filename, &locked);
    QString error;
    if (!whole || !side->boxed.prepare(filename, whole->numPages(),
                                       settings.box, settings.boxClip,
                                       &error)) {
        out << "FAIL: preparing " << filename << ": " << error << "\n";
        ++failures;
        return false;
    }
    side->pdf = side->boxed.load();
    return side->pdf && side->boxed.view(&side->view);
}

static QString size(const QSize &s)
{
    return QString("%1x%2").arg(s.width()).arg(s.height());
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (argc != 2) {
        out << "usage: rendertest DATA_DIRECTORY\n";
        return 2;
    }
    const QString data = QString::fromLocal8Bit(argv[1]);
    RenderSettings settings;
    settings.compareMode = CompareAppearance;
    settings.pen.setStyle(Qt::NoPen);
    settings.pen.setColor(Qt::red);
    settings.brush = QBrush(Qt::red);
    settings.box = PageBox::Trim;

    // trim.pdf and trim-word.pdf: a word changed on line 10, inside the
    // trim box [36 36 576 756], and a mark at 10,10 outside it
    Side a, b;
    if (prepare(data + "/trim.pdf", settings, &a) &&
        prepare(data + "/trim-word.pdf", settings, &b)) {
        const DifferenceRenderer renderer(settings);
        const PagePair pair(0, 0, true, true);
        const QPair<QPixmap, QPixmap> boxes = renderer.pixmaps(
                a.pdf->page(0), "trim", b.pdf->page(0), "trim-word", pair);
        check(boxes.first.size() == QSize(540, 720),
              "the box alone is " + size(boxes.first.size()));
        const QImage image = renderer.pixmaps(a.pdf->page(0), "trim",
                b.pdf->page(0), "trim-word", pair, &a.view, &b.view)
                .first.toImage();
        check(image.size() == QSize(612, 792),
              "the whole page is " + size(image.size()));
        // The mark, black on the page, is dimmed
        const int gray = qGray(image.pixel(15, 777));
        check(gray > 130 && gray < 190,
              QString("the mark outside the box is gray %1").arg(gray));
        check(bluish(image.pixel(36, 400)) || bluish(image.pixel(35, 400)),
              "the box is outlined on its left edge");
        check(qGray(image.pixel(500, 400)) > 250,
              "blank paper inside the box stays white");
        // Highlighting only inside the box, starting on line 10
        const QRect box(36, 36, 540, 720);
        int inside = 0;
        int outside = 0;
        int top = -1;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (reddish(image.pixel(x, y))) {
                    if (box.contains(x, y)) {
                        ++inside;
                        if (top < 0 && x > box.left())
                            top = y;
                    } else {
                        ++outside;
                    }
                }
        check(inside > 0 && outside == 0,
              QString("highlighted pixels: %1 inside the box, %2 outside")
                      .arg(inside).arg(outside));
        check(top > 200 && top < 230,
              QString("the highlighting starts at %1, not on line 10")
                      .arg(top));
    }

    // boxes.pdf page 2 (/Rotate 90): the trim box is 18pt inside the crop
    // box all round, and turns with the page
    Side c, d;
    if (prepare(data + "/boxes.pdf", settings, &c) &&
        prepare(data + "/boxes.pdf", settings, &d)) {
        const DifferenceRenderer renderer(settings);
        const PagePair same(1, 1, false, false);
        const QImage image = renderer.pixmaps(c.pdf->page(1), "c",
                d.pdf->page(1), "d", same, &c.view, &d.view).first.toImage();
        check(image.size() == QSize(756, 576),
              "the rotated page is " + size(image.size()));
        check(bluish(image.pixel(18, 300)) || bluish(image.pixel(17, 300)),
              "rotated: the box is outlined on the left");
        check(bluish(image.pixel(300, 18)) || bluish(image.pixel(300, 17)),
              "rotated: the box is outlined at the top");
        check(!bluish(image.pixel(30, 300)), "rotated: no outline inside");
    }

    if (failures)
        out << failures << " failed\n";
    return failures ? 1 : 0;
}
