/*
    Copyright © 2015 Luca Bellonda. All rights reserved.
    This program or module is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 2 of
    the License, or (at your option) any later version. This program is
    distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
*/


#include "batchcompare.h"

#include "generic.hpp"
#include "mainwindow.hpp"
#include "sequence_matcher.hpp"
#include "textitem.hpp"
#ifdef DEBUG
#include <QtDebug>
#endif
#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPixmapCache>
#include <QPlainTextEdit>
#include <QPrinter>
#include <QSettings>
#include <QUrl>
#include <QXmlStreamWriter>
#include <QBuffer>
#include "aboutform.hpp"

BatchCompare::BatchCompare(const Debug debug,
        const InitialComparisonMode comparisonMode,
        StartupParameters *startupParameters, Status *status, QWidget *parent)
    : QObject(parent),
      savePages(SaveBothPages),
      debug(debug)
{
    _startupParameters = startupParameters ;
    render.compareMode = comparisonMode ;
    render.debug = debug;
    _status = status ;
    cacheSizeMB = 100 ;

    initValues();
    QPixmapCache::setCacheLimit(1000 * qBound(1, cacheSizeMB, 100));
    //writeToSettings("test.ini");
}


void BatchCompare::readFromSettings()
{
    if( _startupParameters->settingsFile().isEmpty()) {
        return ;
    }
    QSettings settings(_startupParameters->settingsFile(), QSettings::IniFormat);
    render.combineTextHighlighting = settings.value("CombineTextHighlighting",true).toBool();
    showHighlight = settings.value("ShowHighlight", -1).toInt();
    render.pen = settings.value("Outline", render.pen).value<QPen>();
    render.brush.setColor(render.pen.color());
    render.brush.setStyle(Qt::SolidPattern);
    render.brush = settings.value("Fill", render.brush).value<QBrush>();
    render.zoning = settings.value("Zoning/Enable", false).toBool();

    render.zoom = settings.value("Zoom", 100).toInt();
    render.columns = settings.value("Columns", 1).toInt();
    render.toleranceR = settings.value("Tolerance/R", 8).toInt();
    render.toleranceY = settings.value("Tolerance/Y", 10).toInt();
    render.excludeMargins = settings.value("Margins/Exclude", false).toBool();
    render.leftMargin = settings.value("Margins/Left", 0).toInt();
    render.rightMargin = settings.value("Margins/Right", 0).toInt();
    render.topMargin = settings.value("Margins/Top", 0).toInt();
    render.bottomMargin = settings.value("Margins/Bottom", 0).toInt();
    cacheSizeMB = settings.value("CacheSizeMB", 25).toInt();
    // 0 keeps one comparison worker per core; a positive value caps them,
    // which also caps peak memory, since every worker opens its own pair
    // of documents.  There is no GUI control: set it in the settings file.
    compareThreads = settings.value("CompareThreads", 0).toInt();
    render.compositionMode = settings.value("compositionMode", -1).toInt();
    render.squareSize = settings.value("SquareSize", render.squareSize).toInt();
    render.ruleWidth = settings.value("RuleWidth", render.ruleWidth).toDouble();
    render.overlap = settings.value("Overlap", render.overlap).toInt();
    render.combineTextHighlighting = settings.value("CombineTextHighlighting", render.combineTextHighlighting).toBool();
    render.opacity = settings.value("Opacity", render.opacity).toInt();
}

void BatchCompare::writeToSettings(const QString &filePath)
{
    //WARNING: this function still to be completed.
    QSettings settings(filePath, QSettings::IniFormat );
    settings.setValue("CombineTextHighlighting", render.combineTextHighlighting);
    settings.setValue("ShowHighlight", showHighlight);
    settings.setValue("Outline", render.pen);
    settings.setValue("Fill", render.brush);
    settings.setValue("Zoning/Enable", render.zoning);

    settings.setValue("Zoom", render.zoom);
    settings.setValue("Columns", render.columns);
    settings.setValue("Tolerance/R", render.toleranceR);
    settings.setValue("Tolerance/Y", render.toleranceY);
    settings.setValue("Margins/Exclude", render.excludeMargins);
    /*
    render.leftMargin = settings.value("Margins/Left", 0).toInt();
    render.rightMargin = settings.value("Margins/Right", 0).toInt();
    render.topMargin = settings.value("Margins/Top", 0).toInt();
    render.bottomMargin = settings.value("Margins/Bottom", 0).toInt();
    cacheSizeMB = settings.value("CacheSizeMB", 25).toInt();
    render.compositionMode = settings.value("compositionMode", -1).toInt();
*/
    settings.setValue("SquareSize", render.squareSize);
    settings.setValue("RuleWidth", render.ruleWidth);

    settings.setValue("Overlap", render.overlap);
    settings.setValue("CombineTextHighlighting", render.combineTextHighlighting);
    settings.setValue("Opacity", render.opacity);

    settings.sync();
}

void BatchCompare::initValues()
{
    render.combineTextHighlighting = true ;
    render.pen.setStyle(Qt::NoPen);
    render.pen.setColor(Qt::red);
    render.brush.setColor(render.pen.color());
    render.brush.setStyle(Qt::SolidPattern);

    render.excludeMargins = false;
    render.zoning = false ;
    render.zoom = 100 ;
    showHighlight = -1 ;
    render.squareSize= 10;
    render.ruleWidth= 1.5;
    render.zoning = false;


    render.columns = 1;
    render.toleranceR = 8 ;
    render.toleranceY = 10 ;
    render.excludeMargins = false;
    render.leftMargin = 0;
    render.rightMargin = 0;
    render.topMargin = 0;
    render.bottomMargin = 0;
    cacheSizeMB = 25;
    compareThreads = 0;
    render.compositionMode = -1;
    render.overlap = 5 ;
    render.combineTextHighlighting = true ;
    render.opacity = 13 ;
}

PdfDocument BatchCompare::getPdf(const QString &filename)
{
    bool locked;
    PdfDocument pdf = loadPdf(filename, &locked);
    if (!pdf) {
        const QString message = locked
                ? tr("Cannot read a locked PDF ('%1').").arg(filename)
                : tr("Cannot load '%1'.").arg(filename);
        _status->setStatusWithDescription(ErrorUnableToLoadFile, message);
        _notifier->messageBox(message);
    }
    return pdf;
}

// The offsets are in pixels of an image rendered at the given DPI
void BatchCompare::batchOperation()
{
    PdfDocument pdf1 = getPdf(_startupParameters->file1());
    if (!pdf1) {
        _status->setStatus(ErrorUnableToLoadFile1);
        return;
    }
    PdfDocument pdf2 = getPdf(_startupParameters->file2());
    if (!pdf2) {
        _status->setStatus(ErrorUnableToLoadFile2);
        return;
    }
    _status->setDoc1Info(docInfo(pdf1, _startupParameters->file1()));
    _status->setDoc2Info(docInfo(pdf2,_startupParameters->file2() ));

    CompareResults results;
    comparePagesBatch( _status, results,
                       _startupParameters->file1(), pdf1,
                      _startupParameters->file2(), pdf2);
    if(!_status->isError() && _startupParameters->isCompareFonts() ) {
        compareFonts(_status->doc1Info(), _status->doc2Info());
    }
    if( _status->isError() && _status->isErrorComparing() ) {
        _status->setPagesNotEqualCount(results.count());
        if( _startupParameters->enablePDFDiff() ) {
            saveResultsBatch(_status, results, pdf1, pdf2 ) ;
        }
    }
}

QList<int> BatchCompare::getPageListBatch( const int which, const PdfDocument &pdf, const int startPageSet)
{
    // Poppler has 0-based page numbers; the UI has 1-based page numbers
    QList<int> pages;

    int startPage = startPageSet-1;
    if( startPage >= pdf->numPages() ) {
        _status->setStatusWithDescription(ErrorInitialPage, tr("file: %1, start page greater than available pages").arg(which) );
        return pages ;
    }
    int count = _startupParameters->pages();
    if( count == StartupParameters::AllPages ) {
        count = pdf->numPages() - startPage ;
    }
    if( (startPage + count ) > pdf->numPages()) {
        _status->setStatusWithDescription(ErrorFinalPage, tr("file: %1, final page is not existing").arg(which) );
        return pages ;
    }
    // 0 -based pages
    for( int index = 0 ; index < count ; index ++ ) {
        pages.append(startPage + index );
    }
    return pages;
}

void BatchCompare::comparePagesBatch(
        Status *status,
        CompareResults &results,
        const QString &filename1, const PdfDocument &pdf1,
        const QString &filename2, const PdfDocument &pdf2)
{
    QList<int> pages1 = getPageListBatch(1, pdf1, _startupParameters->startPage1() );
    if( _status->isError() ) {
        return ;
    }
    QList<int> pages2 = getPageListBatch(2, pdf2, _startupParameters->startPage2() );
    if( _status->isError() ) {
        return ;
    }
    // check for the same number of pages
    if( pages1.count() != pages2.count() ) {
        _status->setStatusWithDescription( ErrorPagesDiffer, tr("the number of pages is not the same on both the documents, doc1:%1, doc2:%2").arg(pages1.count()).arg(pages2.count()));
    }
    render.compareMode = _startupParameters->comparisonMode();
    results.setTotal(qMin(pages1.count(), pages2.count()));
    PageCompareOptions options;
    options.compareAppearance = render.compareMode == CompareAppearance;
    options.excludeMargins = render.excludeMargins;
    options.topMargin = render.topMargin;
    options.bottomMargin = render.bottomMargin;
    options.leftMargin = render.leftMargin;
    options.rightMargin = render.rightMargin;
    options.maxWorkers = compareThreads;
    const QVector<PagePairResult> pairResults = comparePagesInParallel(
            filename1, pdf1, pages1, filename2, pdf2, pages2, options);
    for (int i = 0; i < pairResults.count(); ++i) {
        const PagePairResult &result = pairResults.at(i);
        const int p1 = pages1.at(i);
        const int p2 = pages2.at(i);
        if (result.unreadableFile == 1) {
            _status->setStatusWithDescription( ErrorLoadingPage, tr("Failed to read page %1 from '%2'.").arg(p1 + 1).arg(filename1));
            continue;
        }
        if (result.unreadableFile == 2) {
            _status->setStatusWithDescription( ErrorLoadingPage, tr("Failed to read page %1 from '%2'.").arg(p2 + 1).arg(filename2));
            continue;
        }
        if (result.difference != NoPageDifference) {
            results.differences().append(PagePair(p1, p2, result.difference == VisualPageDifference));
            results.incCount();
            status->setStatusWithDescription( ErrorDocDiffer, tr("documents differ at page: %1").arg(p1+1));
        }
    }
}

void BatchCompare::saveResultsBatch(Status *status, CompareResults &results, const PdfDocument &pdf1, const PdfDocument &pdf2)
{
    int start = 0;
    int end = results.count();
    QString header;
    const QChar bullet(0x2022);
    header = tr("%5 %1 %2 vs. %3 %1 %4").arg(bullet)
        .arg(_startupParameters->file1()).arg(_startupParameters->file2())
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate)).arg(AboutForm::ProgramName);
    if (!DifferenceRenderer(render).saveAsPdf(
            _startupParameters->pdfDiffFilePath(), pdf1,
            _startupParameters->file1(), pdf2, _startupParameters->file2(),
            results.differences().mid(start, end - start), header,
            savePages))
        status->setStatusWithDescriptionUncond(ErrorWritingPDFDiffFile,
                tr("error while writing differences file"));
}

DocInfo *BatchCompare::docInfo(const PdfDocument &pdf, const QString &fileName)
{
    DocInfo *info = new DocInfo();
    info->fileName = QFileInfo(fileName).canonicalPath();
    for (const QString &key : pdf->infoKeys()) {
        if (key == "CreationDate" || key == "ModDate")
            continue;
        info->infos.append(qMakePair( key, pdf->info(key)));
    }
    QDateTime created = pdf->date("CreationDate");
    QDateTime modified = pdf->date("ModDate");
    info->modDate = modified.toString();
    info->creationDate = created.toString();
    info->pageCount = pdf->numPages();
    if (info->pageCount > 0) {
        const double PointToMM = 0.3527777777;
        PdfPage page1 = pdf->page(0);
        QSize size = page1->pageSize();
        info->pageSize = QString("%1pt x %2pt (%3mm x %4mm)")
                  .arg(size.width()).arg(size.height())
                  .arg(qRound(size.width() * PointToMM))
                  .arg(qRound(size.height() * PointToMM));
    }
    { auto v = pdf->getPdfVersion(); info->pdfVersionMajor = v.major; info->pdfVersionMinor = v.minor; }
    // get fonts info
    QList<Poppler::FontInfo> fonts = pdf->fonts();
    for (const Poppler::FontInfo &fi : fonts) {
        LFontInfo *fil = new LFontInfo();
        fil->name = fi.name();
        fil->embedded = fi.isEmbedded();
        fil->subset = fi.isSubset();
        fil->typeName = fi.typeName();
        info->fonts.append(fil);
    }
    return info;
}

void BatchCompare::setNotifier(CompareNotifier *newNotifier)
{
    _notifier = newNotifier;
}


void BatchCompare::writeParam(QXmlStreamWriter & writer, const QString &name, const QString &value)
{
    writer.writeStartElement("item");
    writer.writeAttribute("name", name);
    writer.writeAttribute("value", value);
    writer.writeEndElement();
}

void BatchCompare::writeParam(QXmlStreamWriter & writer, const QString &name, const QVariant &value)
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    QDataStream dataStream(&buffer);
    buffer.open(QFile::WriteOnly);
    dataStream << value ;
    buffer.close();
    QString realValue ;
    for( int i = 0 ; i < byteArray.size() ; i ++ ) {
        QString s = QString::number(0x00FF&byteArray.at(i));
        realValue.append(s);
    }
    writer.writeStartElement("item");
    writer.writeAttribute("name", name);
    writer.writeAttribute("value", realValue.toCaseFolded());
    writer.writeEndElement();
}

static QString toBoolString(const bool value)
{
    if(value) {
        return "true";
    }
    return "false";
}

void BatchCompare::writeParameters(QXmlStreamWriter & writer)
{
    writeParam(writer, "CombineTextHighlighting", toBoolString(render.combineTextHighlighting) );
    writeParam(writer, "ShowHighlight", QString::number(showHighlight) );
    QVariant penVariant(render.pen);
    writeParam(writer, "Outline", penVariant );
    QVariant fillVariant(render.brush);
    writeParam(writer, "Fill", fillVariant );
    writeParam(writer, "Zoom", QString::number(render.zoom) );
    writeParam(writer, "Columns", QString::number(render.columns)  );
    writeParam(writer, "Tolerance/R", QString::number(render.toleranceR) );
    writeParam(writer, "Tolerance/Y", QString::number(render.toleranceY) );
    writeParam(writer, "Zoning/Enable", toBoolString(render.zoning) );
    writeParam(writer, "Margins/Exclude", toBoolString(render.excludeMargins) );
    writeParam(writer, "Margins/Left", QString::number(render.leftMargin) );
    writeParam(writer, "Margins/Right", QString::number(render.rightMargin) );
    writeParam(writer, "Margins/Top", QString::number(render.topMargin) );
    writeParam(writer, "Margins/Bottom", QString::number(render.bottomMargin) );
    writeParam(writer, "CacheSizeMB", QString::number(cacheSizeMB) );
    writeParam(writer, "compositionMode", QString::number(render.compositionMode) );

    writeParam(writer, "SquareSize", QString::number(render.squareSize) );
    writeParam(writer, "RuleWidth", QString::number(render.ruleWidth) );

    writeParam(writer, "Overlap", QString::number(render.overlap) );
    writeParam(writer, "CombineTextHighlighting", toBoolString(render.combineTextHighlighting));
    writeParam(writer, "Opacity", QString::number(render.opacity) );
}

QString BatchCompare::makeFontKey(LFontInfo *l)
{
    QString key = QString("%1:%2:%3:%4").arg(l->name).arg(l->typeName).arg(l->embedded).arg(l->subset);
    return key;
}

QString BatchCompare::makeFontInfoString(LFontInfo *l)
{
    QString key = QString("name:%1, type:%2, embedded:%3, subset:%4").arg(l->name).arg(l->typeName).arg(l->embedded).arg(l->subset);
    return key;
}

bool BatchCompare::compareFonts(DocInfo *doc1, DocInfo *doc2)
{
    bool error = false ;
    QMultiHash<QString,LFontInfo*> fontsDoc2;
    for (LFontInfo *f2 : doc2->fonts) {
        fontsDoc2.insert(makeFontKey(f2), f2);
    }

    for (LFontInfo *f1 : doc1->fonts) {
        QString keyFont1 = makeFontKey(f1);
        if( fontsDoc2.contains(keyFont1)) {
            fontsDoc2.remove(keyFont1, fontsDoc2.values(keyFont1).first());
        } else {
            _status->setStatusWithDescription(ErrorFontsDiffer, QString("Doc 2 is missing font:%1").arg(makeFontInfoString(f1)));
            error = true ;
        }
    }
    // check for
    if( fontsDoc2.keys().size()>0 ) {
        LFontInfo *missing = fontsDoc2.values().first();
        _status->setStatusWithDescription(ErrorFontsDiffer, QString("Doc 1 is missing font:%1").arg(makeFontInfoString(missing)));
        error = true ;
    }
    return !error;
}
