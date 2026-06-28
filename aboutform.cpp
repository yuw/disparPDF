/*
    Copyright © 2011-13 Qtrac Ltd. All rights reserved.
    This program or module is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 2 of
    the License, or (at your option) any later version. This program is
    distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
*/

#include "aboutform.hpp"
// POPPLER_VERSION is supplied via -DPOPPLER_VERSION=... from CMakeLists.txt
#ifndef POPPLER_VERSION
#  define POPPLER_VERSION "unknown"
#endif
#include <QApplication>
#include <QHBoxLayout>
#include <QSettings>
#include <QShortcut>
#include <QTabWidget>
#include <QTextBrowser>

const QString AboutForm::Version("1.2");
#ifdef COMPARA_IS_CONSOLE
const QString AboutForm::ProgramName("disparPDF");
#else
const QString AboutForm::ProgramName("disparPDF");
#endif

AboutForm::AboutForm(QWidget *parent) : QDialog(parent)
{
    QTextBrowser *aboutBrowser = new QTextBrowser;
    aboutBrowser->setReadOnly(true);
    aboutBrowser->setOpenExternalLinks(true);
    aboutBrowser->setHtml(tr(
    "<table border=0>"
    "<tr><td width=90%><b>%1 %2</b> by Yuwsuke Kieda</td>"
         "<td rowspan=6><img align=right src=\":/icon.png\"></td></tr>"
    "<tr><td>Site: <a href='https://github.com/yuw/ConfrontaPDF'>github.com/yuw/ConfrontaPDF</a></td></tr>"
    "<tr><td>This Qt6 port was created with the assistance of AI tools (Claude by Anthropic).</td></tr>"
    "<hr/>"
    "<tr><td><b>%1 %2</b> is based on ConfrontaPDF 1.1 by Luca Bellonda.</td></tr>"
    "<tr><td>ConfrontaPDF is a fork of DiffPDF by Mark Summerfield (2008–2013).</td></tr>"
    "<tr><td>Built with Qt %3 and Poppler %4.</td></tr>"
    "</table><hr>"
    "<p>This program compares the text or the visual appearance of "
    "each page in two PDF files and supports batch operations.</p>"
    "<hr>"
    "<h3>History</h3>"
    "<p>The original open source <b>DiffPDF</b> was created and maintained by "
    "<b>Mark Summerfield</b> 2008–2013.<br/>"
    "Source: <a href='http://www.qtrac.eu/diffpdf-foss.html'>http://www.qtrac.eu/diffpdf-foss.html</a><br/>"
    "Mark's commercial versions: <a href='http://www.qtrac.eu/diffpdf.html'>http://www.qtrac.eu/diffpdf.html</a></p>"
    "<p><b>ConfrontaPDF</b> is a fork by <b>Luca Bellonda</b>, begun in 2015.<br/>"
    "Source: <a href='https://github.com/lbellonda/ConfrontaPDF'>github.com/lbellonda/ConfrontaPDF</a></p>"
    "<p>This <b>Qt6 port (v%2)</b> was created by <b>Yuwsuke Kieda</b> in 2026, "
    "with the assistance of AI tools.<br/>"
    "Source: <a href='https://github.com/yuw/ConfrontaPDF'>github.com/yuw/ConfrontaPDF</a></p>"
    ).arg(qApp->applicationName())
            .arg(Version).arg(qVersion()).arg(POPPLER_VERSION));
    QTextBrowser *contributorsBrowser = new QTextBrowser;
    contributorsBrowser->setReadOnly(true);
    contributorsBrowser->setHtml(tr("<table>"
    "<tr><td>&bull;</td><td><b>Yuwsuke Kieda</b> &mdash; "
    "Qt6 port (2026), with AI assistance (Claude by Anthropic)</td></tr>"
    "<tr><td>&bull;</td><td><b>Luca Bellonda</b> &mdash; "
    "ConfrontaPDF fork (2015)</td></tr>"
    "<tr><td>&bull;</td><td><b>Mark Summerfield</b> &mdash; "
    "wrote the original DiffPDF</td></tr>"
    "<tr><td>&bull;</td><td bgcolor=lightyellow><i>Anonymous Company</i> "
    "&mdash; funded the addition of the margin exclusion "
    "functionality</td></tr>"
    "<tr><td>&bull;</td><td><b>David Paleino</b> &mdash; "
    "Debian packager</td></tr>"
    "<tr><td>&bull;</td><td><b>Dirk Loss</b> &mdash; creating "
    "Mac binaries</td></tr>"
    "<tr><td>&bull;</td><td>Florian Heiderich &mdash; suggested "
    "using composition modes for showing subtle differences</td></tr>"
    "<tr><td>&bull;</td><td><b>Jasmin Blanchette</b> &mdash; "
    "the original idea and subsequent suggestions</td></tr>"
    "<tr><td>&bull;</td><td>Liviu Andronic &mdash; suggested adding "
    "drag and drop</td></tr>"
    "<tr><td>&bull;</td><td>Paul Howarth &mdash; suggestions "
    "resulting in Characters mode</td></tr>"
    "<tr><td>&bull;</td><td bgcolor=\"#F0F0F0\"><i>Pavel Fric</i> &mdash; "
    "Czech translation</td></tr>"
    "<tr><td>&bull;</td><td bgcolor=\"#F0F0F0\"><i>Pierre-Alain "
    "Bandinelli</i>&mdash; French translation</td></tr>"
    "<tr><td>&bull;</td><td bgcolor=\"#F0F0F0\"><i>Rainer Krachten</i> "
    "&mdash; German translation and various suggestions</td></tr>"
    "<tr><td>&bull;</td><td>Rory Gordon &mdash; suggested adding "
    "drag and drop</td></tr>"
    "<tr><td>&bull;</td><td>Bryan Huh &mdash; subtle bug fix"
    "</td></tr>"
    "<tr><td>&bull;</td><td><b>Steven Lee</b> &mdash; creating "
    "Windows binaries</td></tr>"
    "</table>"));
    QTextBrowser *licenceBrowser = new QTextBrowser;
    licenceBrowser->setReadOnly(true);
    licenceBrowser->setHtml(tr(
    "This program is free software: you can redistribute it "
    "and/or modify it under the terms of the GNU General Public License "
    "as published by the Free Software Foundation, either version 2 of "
    "the License, or (at your option), any "
    "later version. This program is distributed in the hope that it will "
    "be useful, but WITHOUT ANY WARRANTY; without even the implied "
    "warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
    "See the GNU General Public License (in file <tt>gpl-2.0.txt</tt>) "
    "for more details."));
    QTabWidget *tabWidget = new QTabWidget;
    tabWidget->addTab(aboutBrowser, tr("&About"));
    tabWidget->addTab(contributorsBrowser, tr("&Contributors"));
    tabWidget->addTab(licenceBrowser, tr("&License"));
    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(tabWidget);
    setLayout(layout);
    resize(520, 486);
    setWindowTitle(tr("%1 — About").arg(AboutForm::ProgramName));
}
