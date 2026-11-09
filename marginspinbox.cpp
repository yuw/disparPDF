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

#include "marginspinbox.hpp"
#include <QLocale>


static const double MillimetresPerPoint = 25.4 / 72.0;


// The widget's locale without group separators: the spin box does not
// show them, and "1,234" would otherwise not read back
static QLocale numberLocale(const QLocale &locale)
{
    QLocale result(locale);
    result.setNumberOptions(QLocale::OmitGroupSeparator);
    return result;
}


MarginSpinBox::MarginSpinBox(QWidget *parent) : QSpinBox(parent)
{
    setSuffix(" pt");
}


QString MarginSpinBox::unitName(const Unit unit)
{
    return unit == Millimetres ? "mm" : "pt";
}


MarginSpinBox::Unit MarginSpinBox::unitFromName(const QString &name)
{
    return name == "mm" ? Millimetres : Points;
}


void MarginSpinBox::setUnit(const Unit unit)
{
    if (unit == m_unit)
        return;
    m_unit = unit;
    // Changing the suffix also redraws the number, now in the new unit
    setSuffix(" " + unitName(unit));
}


void MarginSpinBox::stepBy(int steps)
{
    if (m_unit == Points) {
        QSpinBox::stepBy(steps);
        return;
    }
    // From the nearest whole millimetre, so that the steps run 1, 2, 3 mm
    // rather than by the 0.35 mm of a point.  Whole millimetres survive
    // the round trip through points, so every step moves.
    const int millimetres = qRound(value() * MillimetresPerPoint) + steps;
    setValue(qRound(millimetres / MillimetresPerPoint));
    selectAll();
}


QString MarginSpinBox::textFromValue(int value) const
{
    if (m_unit == Points)
        return QString::number(value);
    // One decimal is enough to tell every point apart (a point is
    // 0.35 mm), so typing a shown value back in gives the same margin
    return numberLocale(locale()).toString(value * MillimetresPerPoint,
                                           'f', 1);
}


int MarginSpinBox::valueFromText(const QString &text) const
{
    const QString number = withoutSuffix(text);
    if (m_unit == Points)
        return number.toInt();
    return qRound(numberLocale(locale()).toDouble(number) /
                  MillimetresPerPoint);
}


QValidator::State MarginSpinBox::validate(QString &input, int &) const
{
    const QString number = withoutSuffix(input);
    if (number.isEmpty())
        return QValidator::Intermediate;
    bool ok = false;
    if (m_unit == Points)
        number.toInt(&ok);
    else {
        const QLocale locale = numberLocale(this->locale());
        locale.toDouble(number, &ok);
        // "12." on the way to "12.5"
        if (!ok && number.endsWith(locale.decimalPoint()))
            return QValidator::Intermediate;
    }
    if (!ok)
        return QValidator::Invalid;
    const int value = valueFromText(number);
    // The minimum is 0, so a smaller value cannot be typed into range,
    // and nor can a larger one by typing more digits
    if (value < minimum() || value > maximum())
        return QValidator::Invalid;
    return QValidator::Acceptable;
}


QString MarginSpinBox::withoutSuffix(const QString &text) const
{
    QString result = text;
    if (!suffix().isEmpty() && result.endsWith(suffix()))
        result.chop(suffix().length());
    return result.trimmed();
}
