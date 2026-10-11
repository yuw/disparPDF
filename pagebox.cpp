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

#include "pagebox.h"
#include <QFile>
#include <qpdf/Buffer.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#include <algorithm>
#include <cmath>
#include <exception>


QString pageBoxName(PageBox box)
{
    switch (box) {
        case PageBox::None: break;
        case PageBox::Media: return "media";
        case PageBox::Crop: return "crop";
        case PageBox::Bleed: return "bleed";
        case PageBox::Trim: return "trim";
        case PageBox::Art: return "art";
    }
    return QString();
}


bool parsePageBox(const QString &name, PageBox *box)
{
    for (PageBox b : {PageBox::Media, PageBox::Crop, PageBox::Bleed,
                      PageBox::Trim, PageBox::Art})
        if (name.compare(pageBoxName(b), Qt::CaseInsensitive) == 0) {
            *box = b;
            return true;
        }
    return false;
}


QString boxClipName(BoxClip clip)
{
    switch (clip) {
        case BoxClip::Media: return "media";
        case BoxClip::Crop: return "crop";
        case BoxClip::None: return "none";
    }
    return QString();
}


bool parseBoxClip(const QString &name, BoxClip *clip)
{
    for (BoxClip c : {BoxClip::Media, BoxClip::Crop, BoxClip::None})
        if (name.compare(boxClipName(c), Qt::CaseInsensitive) == 0) {
            *clip = c;
            return true;
        }
    return false;
}


// 7.9.5: a rectangle may give any two diagonally opposite corners
BoxRect::BoxRect(double x1, double y1, double x2, double y2)
    : llx(std::min(x1, x2)), lly(std::min(y1, y2)),
      urx(std::max(x1, x2)), ury(std::max(y1, y2))
{
}


BoxRect BoxRect::intersected(const BoxRect &other) const
{
    BoxRect r;
    r.llx = std::max(llx, other.llx);
    r.lly = std::max(lly, other.lly);
    r.urx = std::max(r.llx, std::min(urx, other.urx));
    r.ury = std::max(r.lly, std::min(ury, other.ury));
    return r;
}


bool BoxRect::contains(const BoxRect &other) const
{
    return other.llx >= llx && other.lly >= lly &&
           other.urx <= urx && other.ury <= ury;
}


bool BoxRect::fuzzyEquals(const BoxRect &other, double tolerance) const
{
    return std::abs(llx - other.llx) <= tolerance &&
           std::abs(lly - other.lly) <= tolerance &&
           std::abs(urx - other.urx) <= tolerance &&
           std::abs(ury - other.ury) <= tolerance;
}


bool PageBoxes::has(PageBox box) const
{
    switch (box) {
        case PageBox::None: break;
        case PageBox::Media: return true;
        case PageBox::Crop: return hasCrop;
        case PageBox::Bleed: return hasBleed;
        case PageBox::Trim: return hasTrim;
        case PageBox::Art: return hasArt;
    }
    return false;
}


BoxRect PageBoxes::withDefault(PageBox box) const
{
    const BoxRect cropOrDefault = hasCrop ? crop : media;
    switch (box) {
        case PageBox::None: break;
        case PageBox::Media: return media;
        case PageBox::Crop: return cropOrDefault;
        case PageBox::Bleed: return hasBleed ? bleed : cropOrDefault;
        case PageBox::Trim: return hasTrim ? trim : cropOrDefault;
        case PageBox::Art: return hasArt ? art : cropOrDefault;
    }
    return media;
}


BoxRect PageBoxes::visible() const
{
    return withDefault(PageBox::Crop).intersected(media);
}


// /Rotate turns the page clockwise, so the corner of visible() that ends
// up at the top left is its upper left (0), lower left (90), lower right
// (180) or upper right (270)
QRectF PageBoxes::toPageCoordinates(const BoxRect &rect) const
{
    const BoxRect v = visible();
    switch (rotate) {
        case 90:
            return QRectF(rect.lly - v.lly, rect.llx - v.llx,
                          rect.height(), rect.width());
        case 180:
            return QRectF(v.urx - rect.urx, rect.lly - v.lly,
                          rect.width(), rect.height());
        case 270:
            return QRectF(v.ury - rect.ury, v.urx - rect.urx,
                          rect.height(), rect.width());
        default:
            return QRectF(rect.llx - v.llx, v.ury - rect.ury,
                          rect.width(), rect.height());
    }
}


BoxRegion boxRegion(const PageBoxes &boxes, PageBox box, BoxClip clip)
{
    BoxRegion region;
    region.absent = !boxes.has(box);
    const BoxRect chosen = boxes.withDefault(box);
    switch (clip) {
        case BoxClip::Media:
            region.rect = chosen.intersected(boxes.media);
            break;
        case BoxClip::Crop: {
            const BoxRect clipped = chosen.intersected(boxes.media);
            region.rect = clipped.intersected(boxes.visible());
            region.beyondCrop = !boxes.visible().contains(clipped);
            break;
        }
        case BoxClip::None:
            region.rect = chosen;
            region.beyondMedia = !boxes.media.contains(chosen);
            break;
    }
    return region;
}


namespace {

bool readRect(QPDFObjectHandle object, BoxRect *rect)
{
    if (!object.isRectangle())
        return false;
    const QPDFObjectHandle::Rectangle r = object.getArrayAsRectangle();
    *rect = BoxRect(r.llx, r.lly, r.urx, r.ury);
    return true;
}


// Read the file into memory once, so that rewriting it later sees the
// same bytes even if the file has changed in the meantime
void processData(QPDF *pdf, const QByteArray &data, const QString &name)
{
    pdf->setSuppressWarnings(true);
    pdf->processMemoryFile(name.toLocal8Bit().constData(), data.constData(),
                           static_cast<size_t>(data.size()));
}

} // anonymous namespace


bool BoxedDocument::load(const QString &filename, QString *error)
{
    m_data.clear();
    m_boxes.clear();
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }
    m_data = file.readAll();
    try {
        QPDF pdf;
        processData(&pdf, m_data, filename);
        for (QPDFPageObjectHelper &page :
             QPDFPageDocumentHelper(pdf).getAllPages()) {
            PageBoxes boxes;
            if (!readRect(page.getMediaBox(), &boxes.media)) {
                *error = QString("page %1 has no valid MediaBox")
                         .arg(m_boxes.size() + 1);
                m_boxes.clear();
                return false;
            }
            // CropBox and /Rotate are inherited (Table 31); the others
            // are not, and getAttribute() only looks up the page tree for
            // keys that are
            boxes.hasCrop = readRect(page.getAttribute("/CropBox", false),
                                     &boxes.crop);
            QPDFObjectHandle dict = page.getObjectHandle();
            boxes.hasBleed = readRect(dict.getKey("/BleedBox"),
                                      &boxes.bleed);
            boxes.hasTrim = readRect(dict.getKey("/TrimBox"), &boxes.trim);
            boxes.hasArt = readRect(dict.getKey("/ArtBox"), &boxes.art);
            QPDFObjectHandle rotate = page.getAttribute("/Rotate", false);
            if (rotate.isInteger()) {
                const long long r = rotate.getIntValue() % 360;
                boxes.rotate = static_cast<int>(r < 0 ? r + 360 : r);
                if (boxes.rotate % 90)
                    boxes.rotate = 0;  // not a multiple of 90: ignored
            }
            m_boxes.append(boxes);
        }
    } catch (const std::exception &e) {
        *error = QString::fromLocal8Bit(e.what());
        m_boxes.clear();
        return false;
    }
    return true;
}


QByteArray BoxedDocument::rewritten(const QVector<BoxRegion> &regions,
                                    QString *error) const
{
    try {
        QPDF pdf;
        processData(&pdf, m_data, QString());
        std::vector<QPDFPageObjectHelper> pages =
                QPDFPageDocumentHelper(pdf).getAllPages();
        if (static_cast<int>(pages.size()) != regions.size()) {
            *error = QString("%1 regions for %2 pages")
                     .arg(regions.size()).arg(pages.size());
            return QByteArray();
        }
        for (size_t i = 0; i < pages.size(); ++i) {
            const BoxRect &r = regions.at(static_cast<int>(i)).rect;
            QPDFObjectHandle box = QPDFObjectHandle::newFromRectangle(
                    QPDFObjectHandle::Rectangle(r.llx, r.lly, r.urx, r.ury));
            QPDFObjectHandle dict = pages[i].getObjectHandle();
            dict.replaceKey("/MediaBox", box);
            dict.replaceKey("/CropBox", box);
        }
        QPDFWriter writer(pdf);
        writer.setOutputMemory();
        // Copy the streams as they are: only the page dictionaries change
        writer.setDecodeLevel(qpdf_dl_none);
        writer.write();
        std::shared_ptr<Buffer> buffer = writer.getBufferSharedPointer();
        return QByteArray(reinterpret_cast<const char*>(buffer->getBuffer()),
                          static_cast<qsizetype>(buffer->getSize()));
    } catch (const std::exception &e) {
        *error = QString::fromLocal8Bit(e.what());
        return QByteArray();
    }
}
