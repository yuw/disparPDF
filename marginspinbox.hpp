#ifndef MARGINSPINBOX_HPP
#define MARGINSPINBOX_HPP
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

#include <QSpinBox>
#include <QString>

// A spin box for a margin.  Its value() is always in whole points, as
// everything that uses a margin expects -- the comparison, the
// highlighting, the settings and batch mode's INI files -- but it can be
// shown and typed in millimetres.  A value typed in millimetres goes to
// the nearest point, so 20 mm becomes 57 pt and is then shown as
// 20.1 mm: what is shown is the margin that is actually used.
class MarginSpinBox : public QSpinBox
{
public:
    enum Unit {Points, Millimetres};

    explicit MarginSpinBox(QWidget *parent = nullptr);

    Unit unit() const { return m_unit; }
    void setUnit(const Unit unit);

    static QString unitName(const Unit unit);  // "pt" or "mm"
    static Unit unitFromName(const QString &name);

    // In millimetres, a step is a millimetre rather than a point
    void stepBy(int steps) override;

protected:
    QString textFromValue(int value) const override;
    int valueFromText(const QString &text) const override;
    QValidator::State validate(QString &input, int &pos) const override;

private:
    QString withoutSuffix(const QString &text) const;

    Unit m_unit = Points;
};

#endif // MARGINSPINBOX_HPP
