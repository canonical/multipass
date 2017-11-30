/****************************************************************************
**
** Copyright (C) 2013 Laszlo Papp <lpapp@kde.org>
** Copyright (C) 2013 David Faure <faure@kde.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "wrap_text.h"

QString wrapText(const QString& names, int longestOptionNameString, const QString& description)
{
    const QLatin1Char nl('\n');
    QString text = QLatin1String("  ") + names.leftJustified(longestOptionNameString) + QLatin1Char(' ');
    const int indent = text.length();
    int lineStart = 0;
    int lastBreakable = -1;
    const int max = 79 - indent;
    int x = 0;
    const int len = description.length();

    for (int i = 0; i < len; ++i)
    {
        ++x;
        const QChar c = description.at(i);
        if (c.isSpace())
            lastBreakable = i;

        int breakAt = -1;
        int nextLineStart = -1;
        if (x > max && lastBreakable != -1)
        {
            // time to break and we know where
            breakAt = lastBreakable;
            nextLineStart = lastBreakable + 1;
        }
        else if ((x > max - 1 && lastBreakable == -1) || i == len - 1)
        {
            // time to break but found nowhere [-> break here], or end of last line
            breakAt = i + 1;
            nextLineStart = breakAt;
        }
        else if (c == nl)
        {
            // forced break
            breakAt = i;
            nextLineStart = i + 1;
        }

        if (breakAt != -1)
        {
            const int numChars = breakAt - lineStart;
            if (lineStart > 0)
                text += QString(indent, QLatin1Char(' '));
            text += description.midRef(lineStart, numChars);
            text += nl;
            x = 0;
            lastBreakable = -1;
            lineStart = nextLineStart;
            if (lineStart < len && description.at(lineStart).isSpace())
                ++lineStart; // don't start a line with a space
            i = lineStart;
        }
    }

    return text;
}
