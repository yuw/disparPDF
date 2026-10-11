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

// Tests of the window's page box controls, driving MainWindow on Qt's
// offscreen platform, on the PDFs in data/ (see data/make-test-pdfs.sh):
//
//   guitest DATA_DIRECTORY
//
// The settings go to an INI file in a temporary directory, so the user's
// own are left alone.

#include "../label.hpp"
#include "../lineedit.hpp"
#include "../mainwindow.hpp"
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

static QTextStream out(stdout);
static int failures = 0;

static void check(bool ok, const QString &what)
{
    if (!ok) {
        out << "FAIL: " << what << "\n";
        ++failures;
    }
}

// MainWindow's friend: what the tests look at and do
class MainWindowTest
{
public:
    explicit MainWindowTest(MainWindow *window) : w(window) {}

    void setFiles(const QString &file1, const QString &file2)
    {
        w->filename1LineEdit->clear();
        w->filename2LineEdit->clear();
        w->setFile1(file1);
        w->setFile2(file2);
    }
    void setBox(const QString &box, const QString &clip = "media")
    {
        w->boxClipComboBox->setCurrentIndex(
                w->boxClipComboBox->findData(clip));
        w->pageBoxComboBox->setCurrentIndex(
                w->pageBoxComboBox->findData(box));
    }
    void restoreDefault() { w->restoreDefaultPageBox(); }
    void compare() { w->compare(); }
    int differing() const { return w->viewDiffComboBox->count() - 1; }
    bool comparedNothing() const { return w->comparedPages1.isEmpty(); }
    void view(int index)
    {
        w->viewDiffComboBox->setCurrentIndex(index);
        QApplication::processEvents();
    }
    PageBox box() const { return w->pageBox(); }
    BoxClip clip() const { return w->boxClip(); }
    bool clipEnabled() const { return w->boxClipComboBox->isEnabled(); }
    QSize shown() const { return w->page1Label->pixmap().size(); }
    QRect shownBox() const { return w->shownBox[0]; }
    void excludeMargins(bool exclude)
    {
        w->marginsGroupBox->setChecked(exclude);
    }
    QSpinBox *top() const { return w->topMarginSpinBox; }
    QSpinBox *left() const { return w->leftMarginSpinBox; }
    void click(const QPoint &pos) { w->setAMargin(pos); }
    void forgetBoxShown() { w->shownBox[0] = QRect(); }
    void setBoxShown(const QRect &rect) { w->shownBox[0] = rect; }
    QString status() const { return w->statusLabel->text(); }
    QString log() const { return w->logEdit->toPlainText(); }

private:
    MainWindow *w;
};

static QString rect(const QRect &r)
{
    return QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width())
                                 .arg(r.height());
}

static QString saved()
{
    return QSettings().value("PageBox/Box").toString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (argc != 2) {
        out << "usage: guitest DATA_DIRECTORY\n";
        return 2;
    }
    const QString data = QString::fromLocal8Bit(argv[1]);
    QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       settingsDir.path());
    app.setOrganizationName("disparPDF-guitest");
    app.setApplicationName("disparPDF");
    // Nothing should open a message box, but if one does, close it rather
    // than wait for ever
    QTimer closer;
    QObject::connect(&closer, &QTimer::timeout, [] {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            out << "FAIL: a message box opened: " << modal->windowTitle()
                << "\n";
            ++failures;
            modal->close();
        }
    });
    closer.start(100);

    {
        StartupParameters parameters;
        Status status;
        MainWindow window(DebugOff, CompareAppearance, QString(), QString(),
                          "en", &parameters, &status);
        window.show();
        // initialize() is queued, and sets the controls up
        QApplication::processEvents();
        MainWindowTest t(&window);
        check(t.box() == PageBox::None, "the default is the whole page");
        check(!t.clipEnabled(), "Clipped to is off for the whole page");

        // Whole pages differ in a mark outside the trim box; inside it
        // nothing does
        t.setFiles(data + "/trim.pdf", data + "/trim-mark.pdf");
        t.compare();
        check(t.differing() == 1,
              QString("whole pages: %1 differ").arg(t.differing()));
        check(t.top()->maximum() == 386 && t.left()->maximum() == 296,
              QString("whole page margins up to %1pt, %2pt")
                      .arg(t.top()->maximum()).arg(t.left()->maximum()));
        t.setBox("trim");
        check(t.differing() == 0 && t.comparedNothing(),
              "choosing a box forgets the comparison");
        check(t.clipEnabled(), "Clipped to is on with a box");
        // Up to half the 540x720 trim box, less 10pt, at once
        check(t.top()->maximum() == 350 && t.left()->maximum() == 260,
              QString("trim box margins up to %1pt, %2pt")
                      .arg(t.top()->maximum()).arg(t.left()->maximum()));
        t.compare();
        check(t.differing() == 0,
              QString("trim box: %1 differ").arg(t.differing()));

        // The changed word inside it is found, and shown on the page
        t.setFiles(data + "/trim.pdf", data + "/trim-word.pdf");
        t.compare();
        check(t.differing() == 1,
              QString("trim box, changed word: %1 differ")
                      .arg(t.differing()));
        QApplication::processEvents();
        check(t.shown() == QSize(612, 792),
              QString("shown %1x%2").arg(t.shown().width())
                      .arg(t.shown().height()));
        check(t.shownBox() == QRect(36, 36, 540, 720),
              "the box shown at " + rect(t.shownBox()));

        // Margins from the box: 200pt from its top hides the changed line
        t.excludeMargins(true);
        t.top()->setValue(200);
        check(t.top()->value() == 200,
              QString("top margin %1").arg(t.top()->value()));
        t.compare();
        check(t.differing() == 0,
              QString("a 200pt top margin of the trim box: %1 differ")
                      .arg(t.differing()));
        // A click 100px below the box's top sets 100pt, as one 100px below
        // the page's top does without a box
        t.top()->setValue(0);
        t.compare();
        if (t.differing() == 1) {
            t.view(1);
            t.click(QPoint(300, 36 + 100));
            const int inBox = t.top()->value();
            const QRect box = t.shownBox();
            t.forgetBoxShown();
            t.click(QPoint(300, 100));
            const int onPage = t.top()->value();
            t.setBoxShown(box);
            check(inBox == onPage && inBox == 100,
                  QString("a click 100px below the box's top: %1pt; below "
                          "the page's: %2pt").arg(inBox).arg(onPage));
        } else {
            check(false, "without margins the changed word is found");
        }
        t.excludeMargins(false);

        // With and without bleed, lined up on the trim box, with warnings
        t.setFiles(data + "/plain.pdf", data + "/bleed.pdf");
        t.compare();
        check(t.differing() == 0,
              QString("with and without bleed: %1 differ")
                      .arg(t.differing()));
        check(t.status().contains("2 page box warning(s)"),
              "the status: " + t.status());
        check(t.log().contains("Warning: file 1 has no TrimBox"),
              "the warning is in the log");

        // Back to whole pages, and the margins' ranges with them
        t.setFiles(data + "/trim.pdf", data + "/trim-mark.pdf");
        t.restoreDefault();
        check(t.box() == PageBox::None, "Restore Default: the whole page");
        check(t.top()->maximum() == 386,
              QString("whole page margins up to %1pt again")
                      .arg(t.top()->maximum()));
        t.setBox("trim");
        window.close();
    }
    check(saved() == "trim", "saved: " + saved());

    {
        StartupParameters parameters;
        Status status;
        MainWindow window(DebugOff, CompareAppearance, QString(), QString(),
                          "en", &parameters, &status);
        MainWindowTest t(&window);
        check(t.box() == PageBox::Trim, "the box is restored next time");
        t.setBox("art", "none");
        t.restoreDefault();
        check(t.box() == PageBox::None && t.clip() == BoxClip::None,
              "Restore Default leaves the clip alone");
        window.close();
    }
    check(saved() == "none", "saved: " + saved());

    // --box is used for the run, not saved ...
    {
        StartupParameters parameters;
        Status status;
        parameters.parseArgument("--box=bleed", &status);
        MainWindow window(DebugOff, CompareAppearance, QString(), QString(),
                          "en", &parameters, &status);
        check(MainWindowTest(&window).box() == PageBox::Bleed,
              "--box=bleed is used");
        window.close();
    }
    check(saved() == "none", "after --box, saved: " + saved());
    // ... unless it is changed in the window
    {
        StartupParameters parameters;
        Status status;
        parameters.parseArgument("--box=bleed", &status);
        MainWindow window(DebugOff, CompareAppearance, QString(), QString(),
                          "en", &parameters, &status);
        MainWindowTest(&window).setBox("art");
        window.close();
    }
    check(saved() == "art", "changed after --box, saved: " + saved());

    if (failures)
        out << failures << " failed\n";
    return failures ? 1 : 0;
}
