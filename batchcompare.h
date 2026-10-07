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


#ifndef BATCHCOMPARE_H
#define BATCHCOMPARE_H

#include "generic.hpp"
#include "pagecompare.h"
#include "renderer.h"
#include "saveform.hpp"
#include <poppler-qt6.h>
#include <QBrush>
#include <QList>
#include <QPen>
#include <QPainter>
#include "startupparameters.h"
#include "status.h"
#include "compareresults.h"


class CompareNotifier {
public:
    virtual void writeError(const QString &text)= 0 ;
    virtual void writeLine(const QString &text)=0;
    virtual void setOverrideCursor()=0;
    virtual void setRestoreCursor()=0;
    virtual void processEvents()=0;
    virtual void setStatusLabel(const QString &text)=0;
    virtual void messageBox(const QString &text)=0;
};

class BatchCompare : public QObject
{
    Q_OBJECT
public:
    BatchCompare(const Debug debug,
            const InitialComparisonMode comparisonMode,
            StartupParameters *startupParameters,
            Status *status, QWidget *parent=0);
    void batchOperation();
    void readFromSettings();
    void writeToSettings(const QString &filePath);
protected:
    QString finalFileName(const QString &filename);
    DocInfo *docInfo(const PdfDocument &pdf, const QString &fileName);
    void initValues();

private:
    PdfDocument getPdf(const QString &filename);

    // batch operations
    QList<int> getPageListBatch( const int which, const PdfDocument &pdf, const int startPageSet);
    void comparePagesBatch(
            Status *status,
            CompareResults &results,
            const QString &filename1, const PdfDocument &pdf1,
            const QString &filename2, const PdfDocument &pdf2);
    void saveResultsBatch(Status *status, CompareResults &results, const PdfDocument &pdf1, const PdfDocument &pdf2);
    void writeParam(QXmlStreamWriter &writer, const QString &name, const QString &value);
    void writeParam(QXmlStreamWriter & writer, const QString &name, const QVariant &value);

    QString makeFontKey(LFontInfo *l);
    QString makeFontInfoString(LFontInfo *l);
    bool compareFonts(DocInfo *doc1, DocInfo *doc2);


    SavePages savePages;
    Debug debug;
    StartupParameters *_startupParameters;
    Status *_status;
    CompareNotifier *_notifier;

    //---------------
public:
    // How differences are found and shown, from the settings file
    RenderSettings render;
    int cacheSizeMB;
    int compareThreads;
    int showHighlight; // kept in the settings and results files only

    void setNotifier(CompareNotifier *newNotifier);
    void writeParameters(QXmlStreamWriter &writer);
};

#endif // BATCHCOMPARE_H
