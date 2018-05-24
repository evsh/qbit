/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2011  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
*/

#include "loglistwidget.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidgetItem>
#include <QRegExp>

#include "guiiconprovider.h"
#include "theme/fonttheme.h"
#include "theme/themeprovider.h"

LogListWidget::LogListWidget(int maxLines, const Log::MsgTypes &types, QWidget *parent)
    : QListWidget(parent)
    , m_maxLines(maxLines)
    , m_types(types)
{
    // Allow multiple selections
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Context menu
    QAction *copyAct = new QAction(GuiIconProvider::instance()->getIcon("edit-copy"), tr("Copy"), this);
    QAction *clearAct = new QAction(GuiIconProvider::instance()->getIcon("edit-clear"), tr("Clear"), this);
    connect(copyAct, &QAction::triggered, this, &LogListWidget::copySelection);
    connect(clearAct, &QAction::triggered, this, &LogListWidget::clear);
    addAction(copyAct);
    addAction(clearAct);
    setContextMenuPolicy(Qt::ActionsContextMenu);

    connect(&Theme::ThemeProvider::instance(), &Theme::ThemeProvider::fontThemeChanged,
            this, &LogListWidget::applyFontTheme);
}

void LogListWidget::showMsgTypes(const Log::MsgTypes &types)
{
    m_types = types;
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *tempItem = item(i);
        if (!tempItem) continue;

        Log::MsgType itemType = static_cast<Log::MsgType>(tempItem->data(Qt::UserRole).toInt());
        setRowHidden(i, !(m_types & itemType));
    }
}

void LogListWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Copy))
        copySelection();
    else if (event->matches(QKeySequence::SelectAll))
        selectAll();
}

void LogListWidget::applyFontTheme()
{
    QFont font = Theme::FontTheme::current().font(Theme::FontTheme::Element::ExecutionLog);
    for (int row = 0; row < count(); ++row) {
        QListWidgetItem *item = this->item(row);
        QLabel *label = static_cast<QLabel*>(this->itemWidget(item));
        label->setFont(font);
        item->setSizeHint(label->sizeHint());
    }
}

void LogListWidget::appendLine(const QString &line, const Log::MsgType &type)
{
    QListWidgetItem *item = new QListWidgetItem;
    // We need to use QLabel here to support rich text
    QLabel *lbl = new QLabel(line);
    lbl->setFont(Theme::FontTheme::current().font(Theme::FontTheme::Element::ExecutionLog));
    lbl->setContentsMargins(4, 2, 4, 2);
    item->setSizeHint(lbl->sizeHint());
    item->setData(Qt::UserRole, type);
    insertItem(0, item);
    setItemWidget(item, lbl);
    setRowHidden(0, !(m_types & type));

    const int nbLines = count();
    // Limit log size
    if (nbLines > m_maxLines)
        delete takeItem(nbLines - 1);
}

void LogListWidget::copySelection()
{
    static QRegExp htmlTag("<[^>]+>");
    QStringList strings;
    foreach (QListWidgetItem* it, selectedItems())
        strings << static_cast<QLabel*>(itemWidget(it))->text().replace(htmlTag, "");

    QApplication::clipboard()->setText(strings.join("\n"));
}
