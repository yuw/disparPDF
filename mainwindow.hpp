#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP
/*
    Copyright © 2008-13 Qtrac Ltd. All rights reserved.
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
#include "pagecompare.h"
#include "renderer.h"
#include "saveform.hpp"
#include <poppler-qt6.h>
#include <QBrush>
#include <QList>
#include <QMainWindow>
#include <QPen>
#include "startupparameters.h"
#include "status.h"
#include "compareresults.h"
#include "batchcompare.h"

class AboutForm;
class HelpForm;
class Label;
class LineEdit;
class QBoxLayout;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;
class QSpinBox;
class QToolButton;
class QSplitter;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(const Debug debug,
            const InitialComparisonMode comparisonMode,
            const QString &filename1, const QString &filename2,
            const QString &language, StartupParameters *startupParameters,
            Status *status, QWidget *parent=0);

protected:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *object, QEvent *event);
    QString finalFileName(const QString &filename);
    DocInfo *docInfo(const PdfDocument &pdf, const QString &fileName);

private slots:
    void setFile1(QString filename=QString());
    void setFile2(QString filename=QString());
    void setFiles1(const QStringList &filenames);
    void setFiles2(const QStringList &filenames);
    void compare();
    void options();
    void save();
    void about();
    void help();
    void initialize(const QString &filename1, const QString &filename2);
    void updateUi();
    void updateViews(int index=-1);
    void controlDockLocationChanged(Qt::DockWidgetArea area);
    void actionDockLocationChanged(Qt::DockWidgetArea area);
    void zoningDockLocationChanged(Qt::DockWidgetArea area);
    void marginsDockLocationChanged(Qt::DockWidgetArea area);
    void controlTopLevelChanged(bool floating);
    void actionTopLevelChanged(bool floating);
    void zoningTopLevelChanged(bool floating);
    void marginsTopLevelChanged(bool floating);
    void logTopLevelChanged(bool floating);
    void previousPages();
    void nextPages();
    void offsetChanged(int offset);
    void showZones();
    void showMargins();
    void setAMargin(const QPoint &pos);

private:
    void createWidgets(const QString &filename1, const QString &filename2);
    void createCentralArea();
    void createDockWidgets();
    void createConnections();
    void runComparison(const bool verbose, const int pairIndexToShow);
    const QPair<int, int> comparePages(const QString &filename1,
            const PdfDocument &pdf1, const QString &filename2,
            const PdfDocument &pdf2, const bool verbose);
    void comparePrepareUi();
    void compareUpdateUi(const QPair<int, int> &pair, const int millisec,
            const int pairIndexToShow);
    void forgetComparison();
    PagePair pairAt(const int pairIndex) const;
    bool isComparedPair(const int pairIndex) const;
    void showPair(const int pairIndex);
    int differingPairNear(const int pairIndex, const bool after) const;
    void stepPage(const int which, const int delta);
    QString pdfFileFilter() const;
    int writeFileInfo(const QString &filename);
    void writeLine(const QString &text);
    void writeError(const QString &text);
    PdfDocument getPdf(const QString &filename);
    RenderSettings renderSettings() const;
    QList<int> getPageList(int which, const PdfDocument &pdf);
    void showZones(const int Width, const TextBoxList &list,
            QLabel *label);
    void showMargins(QLabel *label);
    void saveAsPdf(const int start, const int end, const PdfDocument &pdf1,
            const PdfDocument &pdf2, const QString &header);
    void saveAsImages(const int start, const int end,
            const PdfDocument &pdf1, const PdfDocument &pdf2,
            const QString &header);

    QPushButton *setFile1Button;
    LineEdit *filename1LineEdit;
    QLabel *comparePages1Label;
    QLineEdit *pages1LineEdit;
    QToolButton *previousPage1Button;
    QToolButton *nextPage1Button;
    Label *page1Label;
    QScrollArea *area1;
    QPushButton *setFile2Button;
    LineEdit *filename2LineEdit;
    QLabel *comparePages2Label;
    QLineEdit *pages2LineEdit;
    QToolButton *previousPage2Button;
    QToolButton *nextPage2Button;
    Label *page2Label;
    QScrollArea *area2;
    QComboBox *compareComboBox;
    QLabel *compareLabel;
    QLabel *viewDiffLabel;
    QPushButton *compareButton;
    QComboBox *viewDiffComboBox;
    QPushButton *previousButton;
    QPushButton *nextButton;
    QLabel *statusLabel;
    QLabel *offsetLabel;
    QSpinBox *offsetSpinBox;
    QLabel *zoomLabel;
    QSpinBox *zoomSpinBox;
    QLabel *showLabel;
    QComboBox *showComboBox;
    QPushButton *optionsButton;
    QPushButton *saveButton;
    QPushButton *aboutButton;
    QPushButton *helpButton;
    QPushButton *quitButton;
    QPlainTextEdit *logEdit;
    QSplitter *splitter;
    QBoxLayout *controlLayout;
    QDockWidget *controlDockWidget;
    QBoxLayout *actionLayout;
    QDockWidget *actionDockWidget;
    QDockWidget *logDockWidget;
    QBoxLayout *compareLayout;
    QGroupBox *zoningGroupBox;
    QLabel *columnsLabel;
    QSpinBox *columnsSpinBox;
    QLabel *toleranceRLabel;
    QSpinBox *toleranceRSpinBox;
    QLabel *toleranceYLabel;
    QSpinBox *toleranceYSpinBox;
    QCheckBox *showZonesCheckBox;
    QBoxLayout *zoningLayout;
    QDockWidget *zoningDockWidget;
    QGroupBox *marginsGroupBox;
    QLabel *topMarginLabel;
    QSpinBox *topMarginSpinBox;
    QLabel *bottomMarginLabel;
    QSpinBox *bottomMarginSpinBox;
    QLabel *leftMarginLabel;
    QSpinBox *leftMarginSpinBox;
    QLabel *rightMarginLabel;
    QSpinBox *rightMarginSpinBox;
    QBoxLayout *marginsLayout;
    QDockWidget *marginsDockWidget;

    QBrush brush;
    QPen pen;
    QString currentPath;
    Qt::DockWidgetArea controlDockArea;
    Qt::DockWidgetArea actionDockArea;
    Qt::DockWidgetArea marginsDockArea;
    Qt::DockWidgetArea zoningDockArea;
    Qt::DockWidgetArea logDockArea;
    std::atomic<bool> cancel;
    // True from the start of a comparison until comparePages() returns,
    // including after Cancel while the workers finish their current pairs.
    // The comparison processes events, and starting another one then
    // would change fingerprints while the first one's workers read it.
    bool comparing;
    // Page fingerprints, kept between comparisons
    PageFingerprintCache fingerprints;
    // The last comparison: page pairIndex of comparedPages1 was paired with
    // page pairIndex + comparedOffset of comparedPages2, with the result
    // pairDifference[pairIndex] (-1 if not compared)
    QList<int> comparedPages1;
    QList<int> comparedPages2;
    int comparedOffset = 0;
    QVector<int> pairDifference;
    int viewedPairIndex = -1; // -1 if none
    QString comparisonSummary; // e.g., "3 differ 10/10 compared"
    bool requirePdfExtension; // only offer files named *.pdf
    bool showToolTips;
    bool combineTextHighlighting;
    QString saveFilename;
    bool saveAll;
    SavePages savePages;
    const QString language;
    Debug debug;
    AboutForm *aboutForm;
    HelpForm *helpForm;
    StartupParameters *_startupParameters;
    Status *_status;
    int currentCompareIndex;
};

#endif // MAINWINDOW_HPP

