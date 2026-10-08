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

#include "mainwindow.hpp"
#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTextStream>
#include <QTranslator>
#include "aboutform.hpp"
#include "batchcompare.h"
#include "commandlinemanager.h"

// Run as disparPDFc (e.g., through a link to disparPDF, or a copy of it,
// with that name), the program is always in batch mode.  The name it was
// run by is taken from argv[0]: QCoreApplication::applicationFilePath()
// resolves symbolic links, so would always give disparPDF.
static bool runAsConsole(const char *argv0)
{
    QString name = QFileInfo(QString::fromLocal8Bit(argv0)).fileName();
    if (name.endsWith(".exe", Qt::CaseInsensitive))
        name.chop(4);
    return name.compare("disparPDFc", Qt::CaseInsensitive) == 0;
}

int main(int argc, char *argv[])
{
    const bool console = argc > 0 && runAsConsole(argv[0]);
    // The name of this program, for --help and --version
    const char *const CommandName = console ? "disparPDFc" : "disparPDF";
    StartupParameters startupParameters;
    // Batch mode and --help show no window, so they should work without a
    // display: use Qt's offscreen platform for them unless one was chosen
    // (the last of --batch and --interactive wins)
    bool batch = console;
    bool helpOrVersion = false;
    bool platformGiven = !qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM");
    for (int i = 1; i < argc; ++i) {
        const QByteArray arg(argv[i]);
        if (arg == "--")
            break;
        if (arg == "-b" || arg == "--batch")
            batch = true;
        else if (arg == "--interactive")
            batch = false;
        else if (arg == "-h" || arg == "--help" || arg == "--version")
            helpOrVersion = true;
        if (arg == "-platform" || arg.startsWith("-platform="))
            platformGiven = true;
    }
    if ((batch || helpOrVersion) && !platformGiven)
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
#ifdef Q_OS_MACOS
    app.setCursorFlashTime(0);
#endif
    app.setOrganizationName("disparPDF");
    app.setOrganizationDomain("disparPDF");
    app.setApplicationName(AboutForm::ProgramName);
    app.setWindowIcon(QIcon(":/icon.png"));
    // Lets Wayland desktops find disparPDF.desktop, and so the icon
    app.setDesktopFileName("disparPDF");
    QTextStream out(stdout);
    QStringList args = app.arguments().mid(1);
    QSettings settings;
    InitialComparisonMode comparisonMode = static_cast<
            InitialComparisonMode>(settings.value("InitialComparisonMode",
                        CompareWords).toInt());
    const QString LanguageOption = "--language=";
    QString filename1;
    QString filename2;
    QString language = QLocale::system().name();
    bool optionsOK = true;
    Debug debug = DebugOff;
    Status status ;
    bool seenCompareType = false;
    QStringList errors;
    QStringList fileArguments;

    if (console)
        startupParameters.setIsBatch(true);
    for (const QString &arg : std::as_const(args)) {
        if (optionsOK && (arg == "--appearance" || arg == "-a")) {
            comparisonMode = CompareAppearance;
            seenCompareType = true ;
        } else if (optionsOK && (arg == "--characters" || arg == "-c")) {
            comparisonMode = CompareCharacters;
            seenCompareType = true ;
        } else if (optionsOK && (arg == "--words" || arg == "-w")) {
            comparisonMode = CompareWords;
            seenCompareType = true ;
        } else if (optionsOK && arg.startsWith(LanguageOption))
            language = arg.mid(LanguageOption.length());
        else if (optionsOK && arg == "--version") {
            out << CommandName << " " << AboutForm::Version << "\n"
                << "Built with Qt " << QT_VERSION_STR << " and Poppler "
                << POPPLER_VERSION << "\n"
                << "License GPLv2+: GNU GPL version 2 or later "
                   "<https://gnu.org/licenses/gpl.html>\n";
            return 0;
        }
        else if (optionsOK && (arg == "--help" || arg == "-h")) {
            // GNU style, which help2man turns into the manual pages
            if (console)
                out << "Usage: " << CommandName << " [OPTION]... FILE1 FILE2\n"
                    "Compare two PDF files without a window, and print the "
                    "result code\n(see below).\n"
                    "\n"
                    "Options:\n"
                    "  -a, --appearance       compare the appearance of the "
                    "pages (the default)\n"
                    "  -c, --characters       compare the text character by "
                    "character\n"
                    "  -w, --words            compare the text word by word\n";
            else
                out << "Usage: " << CommandName << " [OPTION]... [FILE1 [FILE2]]\n"
                    "  or:  " << CommandName << " --batch [OPTION]... FILE1 FILE2\n"
                    "  or:  disparPDFc [OPTION]... FILE1 FILE2\n"
                    "Compare two PDF files and show their differences.\n"
                    "\n"
                    "The files are optional and are normally chosen in the "
                    "window.  With --batch,\nthey are compared without a "
                    "window, and the result code (see below) is\nprinted.\n"
                    "\n"
                    "Options:\n"
                    "  -a, --appearance       compare the appearance of the "
                    "pages (the default in\n"
                    "                           batch mode)\n"
                    "  -c, --characters       compare the text character by "
                    "character\n"
                    "  -w, --words            compare the text word by word "
                    "(the default otherwise)\n";
            out << "      --any-extension    accept files whose names do "
                "not end in .pdf\n"
                "      --language=LANG    use the given translation "
                "language, e.g., en for\n"
                "                           English, cz for Czech; "
                "English is used if there is\n"
                "                           no translation\n"
                "      --debug=2          write the text fed to the "
                "sequence matcher into\n"
                "                           temporary files (e.g., "
                "/tmp/page1.txt)\n"
                "      --debug=3          as --debug=2, but also with "
                "the coordinates, in y, x\n"
                "                           order\n"
                "  -h, --help             display this help and exit\n"
                "      --version          output version information and "
                "exit\n"
                "\n"
                "Batch mode options:\n";
            if (console)
                out << "      --interactive      show the window, as "
                    "disparPDF does\n";
            else
                out << "  -b, --batch            compare without a window\n"
                    "      --interactive      show the window (the default, "
                    "except as disparPDFc)\n";
            out << "      --outType=0        print only the result code "
                "(the default)\n"
                "      --outType=1        print the result code and a "
                "description\n"
                "      --pages=N          compare N pages (default all)\n"
                "      --startPage1=N     start at page N of FILE1\n"
                "      --startPage2=N     start at page N of FILE2\n"
                "      --pdfdiff=FILE     save the pages that differ, "
                "highlighted, to FILE\n"
                "      --xmlResult=FILE   write the result, in XML, to "
                "FILE\n"
                "      --key=KEY          record KEY in the XML result\n"
                "      --settings=FILE    read settings (zoom, margins, "
                "tolerances, colors,\n"
                "                           etc.) from FILE, in INI "
                "format, instead of using\n"
                "                           the defaults\n"
                "      --compareFonts     also compare the fonts the "
                "files use\n"
                "\n"
                "Result codes, printed in batch mode and also the exit "
                "status (modulo 256):\n"
                "   0  the files are the same\n"
                "   1  the files differ\n"
                "   2  the files have different numbers of pages\n"
                "   3  a start page is beyond the end of its file\n"
                "   4  a file has fewer pages than --pages asks for\n"
                "   5  the same file was given twice\n"
                "   6  the files use different fonts (with "
                "--compareFonts)\n"
                "  -1  a command line error\n"
                "  -3, -4  FILE1 or FILE2 cannot be read\n"
                "  -5  a page cannot be read\n"
                "  -6, -7  the --pdfdiff or --xmlResult file cannot be "
                "written\n";
            if (!console)
                out << "\n"
                    "In the window, press F1 for the full documentation, and "
                    "click About for the\ncopyright and license details.\n";
            return 0;
        }
        else if (optionsOK && (arg == "--debug" || arg == "--debug=1" ||
                               arg == "--debug1"))
            ; // basic debug mode currently does nothing (did show zones)
        else if (optionsOK && (arg == "--debug=2" || arg == "--debug2"))
            debug = DebugShowTexts;
        else if (optionsOK && (arg == "--debug=3" || arg == "--debug3"))
            debug = DebugShowTextsAndYX;
        else if (optionsOK && arg == "--") {
            optionsOK = false;
        } else if (optionsOK && startupParameters.parseArgument(arg, &status) ) {
            ; // empty statement
        } else if (optionsOK && arg.startsWith('-') && arg != "-") {
            errors << arg ; // An unknown option
        } else {
            fileArguments << arg ;
        }
    }
    // The first two arguments that are not options are the files to
    // compare.  Their names must end in .pdf, unless --any-extension is
    // given or, in the GUI, the Options dialog says otherwise.
    const bool requirePdfExtension = !startupParameters.anyExtension() &&
            (startupParameters.isBatch() ||
             settings.value("RequirePdfExtension", true).toBool());
    for (const QString &arg : std::as_const(fileArguments)) {
        const bool acceptable = !requirePdfExtension ||
                                arg.toLower().endsWith(".pdf");
        if (acceptable && filename1.isEmpty()) {
            filename1 = arg;
            startupParameters.setFile1(filename1);
        } else if (acceptable && filename2.isEmpty()) {
            filename2 = arg;
            startupParameters.setFile2(filename2);
        } else {
            errors << arg ;
        }
    }
    // The translators must outlive this block: a QTranslator uninstalls
    // itself when destroyed
    QTranslator qtTranslator;
    QTranslator appTranslator;
    if(!startupParameters.isBatch()) {
        QString translationsPath =
            QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        if (qtTranslator.load("qt_" + language, translationsPath))
            app.installTranslator(&qtTranslator);
        if (appTranslator.load("disparPDF_" + language, ":/"))
            app.installTranslator(&appTranslator);
    }

    if( errors.count() > 0 ) {
        if( startupParameters.isBatch() ) {
            status.setParamError( true, errors.first());
            return status.returnOp(startupParameters.returnType(), &startupParameters);
        } else {
            for (const QString &str : std::as_const(errors)) {
                out << QObject::tr("unrecognized argument '") << str << "'\n";
            }
        }
    }
    if( startupParameters.isBatch() && !seenCompareType ) {
        comparisonMode = CompareAppearance;
    }
    startupParameters.setComparisonMode(comparisonMode);
    if( status.isError() ) {
        return status.returnOp(startupParameters.returnType(), &startupParameters);
    }
    if(!startupParameters.isBatch()) {
        MainWindow window(debug, comparisonMode, filename1, filename2,
                language.left(2), &startupParameters, &status ); // We want de not de_DE etc.
        window.show();
        app.exec();
        return 0;
    } else {
        if( !startupParameters.validate(&status) ) {
            return status.returnOp(startupParameters.returnType(), &startupParameters);
        }
        CommandLineManager manager(debug, comparisonMode,
                &startupParameters, &status );
        manager.batchOperation();
        return status.returnOp(startupParameters.returnType(), &startupParameters, manager.getCompare() );
    }
}
