/*
 * This file is part of PATM.
 *
 * PATM (Pipeline Automation Tool Manager) is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * PATM is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PATM. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tooleditor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>

#include "c_backend.h"
#include "core/log.h"

#define USER_TOOLS_SUBDIR "patm/tools"

static QString userToolPath(const QString &basename)
{
    QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
    QByteArray home = qgetenv("HOME");
    QString path;
    if (!xdg.isEmpty())
        path = QString::fromUtf8(xdg) + "/" + USER_TOOLS_SUBDIR + "/" + basename;
    else if (!home.isEmpty())
        path = QString::fromUtf8(home) + "/.config/" + USER_TOOLS_SUBDIR + "/" + basename;
    else
        return QString();

    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
}

static QString baseName(const QString &path)
{
    int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.mid(slash + 1) : path;
}

ToolEditor::ToolEditor(const QString &toolPath, QWidget *parent)
    : QDialog(parent), m_originalPath(toolPath), m_userPath(userToolPath(baseName(toolPath)))
{
    setWindowTitle(QString("Tool editor - %1").arg(baseName(toolPath)));
    setMinimumSize(760, 560);

    QVBoxLayout *vbox = new QVBoxLayout(this);

    m_editor = new QPlainTextEdit;
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_editor->setFont(mono);
    m_editor->setTabStopDistance(QFontMetricsF(mono).horizontalAdvance(' ') * 4);
    vbox->addWidget(m_editor, 1);

    QHBoxLayout *hbox = new QHBoxLayout;
    QPushButton *saveBtn = new QPushButton("Save");
    QPushButton *revertBtn = new QPushButton("Revert");
    QPushButton *closeBtn = new QPushButton("Close");
    m_status = new QLabel;
    m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    hbox->addWidget(saveBtn);
    hbox->addWidget(revertBtn);
    hbox->addWidget(closeBtn);
    hbox->addWidget(m_status, 1);
    vbox->addLayout(hbox);

    connect(saveBtn, &QPushButton::clicked, this, &ToolEditor::onSave);
    connect(revertBtn, &QPushButton::clicked, this, &ToolEditor::onRevert);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    loadFile();
}

void ToolEditor::loadFile()
{
    QFile f(m_originalPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_status->setText("Cannot open file for reading.");
        return;
    }
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    QString header = QString("Editing: %1%2")
                         .arg(baseName(m_originalPath))
                         .arg(m_originalPath == m_userPath ? "" : "  (Save writes your own copy)");
    m_status->setText(header);
}

void ToolEditor::onSave()
{
    QFile f(m_userPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        m_status->setText("Cannot write to your tools directory.");
        return;
    }
    f.write(m_editor->toPlainText().toUtf8());
    f.close();
    m_status->setText("Saved. The runner lists your copy.");
    PATM_LOG_INFO("tool saved to %s", m_userPath.toUtf8().constData());
}

void ToolEditor::onRevert()
{
    loadFile();
}
