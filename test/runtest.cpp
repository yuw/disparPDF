/*
    This program or module is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 2 of
    the License, or (at your option) any later version. This program is
    distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
*/

// Runs one disparPDFc test:
//
//   runtest DISPARPDFC RESULT PAGES [OPTION]... -- [ARGUMENT]...
//
// runs "DISPARPDFC -b --outType=0 --pdfdiff=OUT ARGUMENT..." with no
// display, and checks that the result code it prints is RESULT (e.g., 0
// same, 1 different, 2 different page counts; "-" to skip) and, unless
// PAGES is "-", that it saved PAGES pages of differences to OUT (0
// meaning no file at all).  Options:
//   --setting KEY=VALUE   pass a settings file with KEY=VALUE in it
//   --stdout TEXT         check that standard output contains TEXT

#include <poppler-qt6.h>
#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

static QTextStream err(stderr);

static int fail(const QString &message)
{
    err << "FAIL: " << message << "\n";
    return 1;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);
    if (args.count() < 4 || !args.contains("--")) {
        err << "usage: runtest DISPARPDFC RESULT PAGES [OPTION]... -- "
               "[ARGUMENT]...\n";
        return 2;
    }
    const QString program = args.takeFirst();
    const QString expectedResult = args.takeFirst();
    const QString expectedPages = args.takeFirst();

    QTemporaryDir dir;
    if (!dir.isValid())
        return fail("cannot create a temporary directory");
    const QString settingsFile = dir.filePath("settings.ini");
    bool haveSettings = false;
    QString expectedOutput;
    while (!args.isEmpty() && args.first() != "--") {
        const QString option = args.takeFirst();
        if (args.isEmpty() || args.first() == "--")
            return fail("missing value for " + option);
        const QString value = args.takeFirst();
        if (option == "--setting") {
            const int equals = value.indexOf('=');
            QSettings settings(settingsFile, QSettings::IniFormat);
            settings.setValue(value.left(equals), value.mid(equals + 1));
            haveSettings = true;
        }
        else if (option == "--stdout")
            expectedOutput = value;
        else
            return fail("unknown option " + option);
    }
    args.removeFirst(); // "--"

    const QString output = dir.filePath("diff.pdf");
    QStringList arguments;
    arguments << "-b" << "--outType=0" << "--pdfdiff=" + output;
    if (haveSettings)
        arguments << "--settings=" + settingsFile;
    arguments << args;
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // No display: batch mode must manage without one
    environment.remove("DISPLAY");
    environment.remove("WAYLAND_DISPLAY");
    environment.remove("QT_QPA_PLATFORM");
    environment.insert("HOME", dir.path());
    environment.insert("XDG_CONFIG_HOME", dir.filePath("config"));
    process.setProcessEnvironment(environment);
    process.start(program, arguments);
    if (!process.waitForFinished(300000))
        return fail("disparPDFc did not finish: " + process.errorString());
    const QString standardOutput = QString::fromLocal8Bit(
            process.readAllStandardOutput());
    const QString standardError = QString::fromLocal8Bit(
            process.readAllStandardError());
    if (!standardError.isEmpty())
        err << "disparPDFc's standard error:\n" << standardError;

    if (process.exitStatus() != QProcess::NormalExit)
        return fail("disparPDFc crashed");
    const QString result = standardOutput.section('\n', 0, 0).trimmed();
    if (expectedResult != "-" && result != expectedResult)
        return fail(QString("result %1, expected %2").arg(result)
                    .arg(expectedResult));
    if (!expectedOutput.isEmpty() &&
        !standardOutput.contains(expectedOutput))
        return fail("standard output does not contain '" +
                    expectedOutput + "':\n" + standardOutput);
    if (expectedPages != "-") {
        int pages = 0;
        if (QFile::exists(output)) {
            std::unique_ptr<Poppler::Document> pdf =
                    Poppler::Document::load(output);
            if (!pdf)
                return fail("cannot read the saved differences");
            pages = pdf->numPages();
        }
        if (pages != expectedPages.toInt())
            return fail(QString("%1 pages of differences saved, "
                                "expected %2").arg(pages)
                        .arg(expectedPages));
    }
    return 0;
}
