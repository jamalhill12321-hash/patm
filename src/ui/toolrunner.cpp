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

#include "toolrunner.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QFrame>
#include <QScrollBar>
#include <QTextCursor>
#include <QVBoxLayout>

#include "c_backend.h"
#include "mainwindow.h"
#include "tooleditor.h"

#define MAX_TOOLS 64

static QString baseName(const QString &path)
{
    int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.mid(slash + 1) : path;
}

static void addPyFiles(const char *dir, QStringList &list)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != nullptr && list.size() < MAX_TOOLS) {
        size_t len = strlen(ent->d_name);
        if (len < 4 || strcmp(ent->d_name + len - 3, ".py") != 0 || ent->d_name[0] == '.')
            continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        QString bn = QString::fromUtf8(ent->d_name);
        bool dup = false;
        for (const QString &existing : list) {
            if (baseName(existing) == bn) { dup = true; break; }
        }
        if (!dup) list.append(QString::fromUtf8(path));
    }
    closedir(d);
}

static void scanToolPaths(QStringList &list)
{
    list.clear();
    QByteArray xdg = qgetenv("XDG_CONFIG_HOME");
    QByteArray home = qgetenv("HOME");
    QString userDir;
    if (!xdg.isEmpty()) userDir = QString::fromUtf8(xdg) + "/patm/tools";
    else if (!home.isEmpty()) userDir = QString::fromUtf8(home) + "/.config/patm/tools";
    if (!userDir.isEmpty()) addPyFiles(userDir.toUtf8().constData(), list);

    char exe[1024];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash) *slash = '\0';

        char d1[1080];
        snprintf(d1, sizeof(d1), "%s/tools", exe);
        addPyFiles(d1, list);
        snprintf(d1, sizeof(d1), "%s/../tools", exe);
        addPyFiles(d1, list);

        char *prev = strrchr(exe, '/');
        for (int up = 0; up < 4 && prev; up++) {
            *prev = '\0';
            prev = strrchr(exe, '/');
            if (!prev) break;
            snprintf(d1, sizeof(d1), "%s/tools", exe);
            addPyFiles(d1, list);
        }
    }

    // also check common install paths
    addPyFiles("/opt/patm/tools", list);
    addPyFiles("/usr/local/share/patm/tools", list);
    addPyFiles("/usr/share/patm/tools", list);

    // TODO: maybe also check XDG_DATA_DIRS?
    if (list.isEmpty()) {
        QString cwd = QDir::currentPath();
        addPyFiles((cwd + "/tools").toUtf8().constData(), list);
    }
}

static PatmError connectWithParams(const char *host, int port, const char *user,
                                   const char *pass, const char *dbname, int engine,
                                   const char *ssl_mode, PatmConn **out)
{
    PatmConnParams params = {
        (PatmDbEngine)engine, host, user, pass, dbname,
        port > 0 ? port : patm_db_driver_get((PatmDbEngine)engine)->default_port,
        ssl_mode && ssl_mode[0] ? ssl_mode : PATM_SSL_DISABLE,
    };
    return patm_db_connect(&params, out);
}

ToolRunner::ToolRunner(QWidget *parent)
    : QWidget(parent)
{
    m_cfg = MainWindow::config();

    QStringList toolPaths;
    scanToolPaths(toolPaths);
    m_toolPaths = toolPaths;

    m_knownTools.append({"export_csv.py", "Export a table to CSV",
                         {{"table", "Table:", ""}, {"output", "Output file:", "export.csv"}, {"batch_size", "Batch size:", "1000"}}});
    m_knownTools.append({"transfer_table.py", "Copy a table between connections",
                         {{"source_table", "Source table:", ""}, {"target_table", "Target table:", ""}, {"batch_size", "Batch size:", "500"}}});

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);

    // tool selection
    QHBoxLayout *row0 = new QHBoxLayout;
    QLabel *toolLbl = new QLabel("Tool:");
    toolLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_toolCombo = new QComboBox;
    for (const QString &path : m_toolPaths) {
        QString bn = baseName(path);
        if (path.contains("/.config/") || path.contains("/patm/tools/"))
            bn += " (yours)";
        m_toolCombo->addItem(bn);
    }
    if (!m_toolPaths.isEmpty()) m_toolCombo->setCurrentIndex(0);
    row0->addWidget(toolLbl);
    row0->addWidget(m_toolCombo, 1);
    vbox->addLayout(row0);

    // source connection
    QHBoxLayout *row1 = new QHBoxLayout;
    QLabel *srcLbl = new QLabel("Source:");
    srcLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sourceCombo = new QComboBox;
    for (size_t i = 0; i < m_cfg->nconns; i++)
        m_sourceCombo->addItem(QString::fromUtf8(m_cfg->conns[i].id));
    row1->addWidget(srcLbl);
    row1->addWidget(m_sourceCombo, 1);
    vbox->addLayout(row1);

    // params
    m_paramWidget = new QWidget;
    m_paramLayout = new QFormLayout(m_paramWidget);
    m_paramLayout->setContentsMargins(0, 0, 0, 0);
    vbox->addWidget(m_paramWidget);

    // buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *runBtn = new QPushButton("Run");
    QPushButton *editBtn = new QPushButton("Edit Tool");
    btnRow->addWidget(runBtn);
    btnRow->addWidget(editBtn);
    btnRow->addStretch();
    vbox->addLayout(btnRow);

    // status
    m_status = new QLabel;
    m_status->setAlignment(Qt::AlignLeft);
    m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    vbox->addWidget(m_status);

    // log output
    m_logView = new QPlainTextEdit;
    m_logView->setReadOnly(true);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_logView->setFont(mono);
    vbox->addWidget(m_logView, 1);

    // target connection (collapsible)
    QCheckBox *tgtToggle = new QCheckBox("Show target connection");
    vbox->addWidget(tgtToggle);

    QGroupBox *tgtGroup = new QGroupBox("Target connection");
    tgtGroup->setCheckable(false);
    QFormLayout *tgtGrid = new QFormLayout(tgtGroup);
    m_tgtHost = new QLineEdit; m_tgtHost->setPlaceholderText("127.0.0.1");
    m_tgtPort = new QLineEdit; m_tgtPort->setPlaceholderText("5432");
    m_tgtUser = new QLineEdit; m_tgtUser->setPlaceholderText("postgres");
    m_tgtPass = new QLineEdit; m_tgtPass->setEchoMode(QLineEdit::Password);
    m_tgtDb = new QLineEdit;
    tgtGrid->addRow("Host:", m_tgtHost);
    tgtGrid->addRow("Port:", m_tgtPort);
    tgtGrid->addRow("User:", m_tgtUser);
    tgtGrid->addRow("Password:", m_tgtPass);
    tgtGrid->addRow("Database:", m_tgtDb);
    m_tgtEngine = new QComboBox;
    m_tgtEngine->addItems({"PostgreSQL", "MySQL", "MariaDB"});
    tgtGrid->addRow("Engine:", m_tgtEngine);
    tgtGroup->setVisible(false);
    vbox->addWidget(tgtGroup);

    connect(tgtToggle, &QCheckBox::toggled, tgtGroup, &QWidget::setVisible);

    if (m_cfg->nconns == 0)
        m_status->setText("No connections configured yet.");

    m_toolCombo->blockSignals(true);
    m_sourceCombo->blockSignals(true);

    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolRunner::onToolChanged);
    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ToolRunner::onSourceChanged);
    connect(runBtn, &QPushButton::clicked, this, &ToolRunner::onRun);
    connect(editBtn, &QPushButton::clicked, this, &ToolRunner::onEditTool);

    rebuildParams();

    m_toolCombo->blockSignals(false);
    m_sourceCombo->blockSignals(false);
}

void ToolRunner::clearParamGrid()
{
    QFormLayout *fl = qobject_cast<QFormLayout *>(m_paramWidget->layout());
    if (!fl) return;
    while (fl->count() > 0) {
        QLayoutItem *item = fl->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    memset(m_paramEntries, 0, sizeof(m_paramEntries));
    m_tableCombo = nullptr;
}

void ToolRunner::rebuildParams()
{
    int sel = m_toolCombo->currentIndex();
    if (sel < 0 || sel >= m_toolPaths.size()) { clearParamGrid(); return; }

    QString toolBn = baseName(m_toolPaths[sel]);
    const ToolMeta *meta = nullptr;
    for (const auto &t : m_knownTools) {
        if (t.name == toolBn) { meta = &t; break; }
    }

    clearParamGrid();
    if (!meta || meta->params.isEmpty()) {
        QLabel *lbl = new QLabel("No parameters for this tool.");
        lbl->setAlignment(Qt::AlignLeft);
        qobject_cast<QFormLayout *>(m_paramWidget->layout())->addRow(lbl);
        return;
    }

    bool isExportCSV = !meta->params.isEmpty() && meta->params[0].key == "table";
    bool isTransferTable = !meta->params.isEmpty() && meta->params[0].key == "source_table";
    QFormLayout *fl = qobject_cast<QFormLayout *>(m_paramWidget->layout());

    for (int i = 0; i < meta->params.size() && i < 8; i++) {
        if (isExportCSV && i == 0) {
            m_tableCombo = new QComboBox;
            refreshTableList();
            fl->addRow(meta->params[i].label + ":", m_tableCombo);
            continue;
        }

        if (isTransferTable && i == 0) {
            m_tableCombo = new QComboBox;
            refreshTableList();
            fl->addRow(meta->params[i].label + ":", m_tableCombo);
            continue;
        }

        QLineEdit *entry = new QLineEdit;
        if (!meta->params[i].defval.isEmpty())
            entry->setText(meta->params[i].defval);
        else
            entry->setPlaceholderText("(required)");

        if (isExportCSV && meta->params[i].key == "output") {
            QHBoxLayout *hbox = new QHBoxLayout;
            hbox->addWidget(entry);
            QPushButton *browseBtn = new QPushButton("Browse...");
            connect(browseBtn, &QPushButton::clicked, this, &ToolRunner::onBrowseOutput);
            hbox->addWidget(browseBtn);
            QWidget *container = new QWidget;
            container->setLayout(hbox);
            fl->addRow(meta->params[i].label + ":", container);
            m_paramEntries[i] = entry;
            continue;
        }

        fl->addRow(meta->params[i].label + ":", entry);
        m_paramEntries[i] = entry;
    }
}

void ToolRunner::onToolChanged(int index)
{
    if (index < 0 || index >= m_toolPaths.size()) return;
    QString bn = baseName(m_toolPaths[index]);
    for (const auto &t : m_knownTools) {
        if (t.name == bn) { m_status->setText(t.desc); rebuildParams(); return; }
    }
    m_status->setText(QString());
    rebuildParams();
}

void ToolRunner::onSourceChanged(int index)
{
    Q_UNUSED(index);
    refreshTableList();
}

void ToolRunner::refreshTableList()
{
    if (!m_tableCombo) return;
    m_tableCombo->clear();

    PatmConn *conn = MainWindow::activeConnection();
    if (!conn) {
        m_status->setText("Not connected. Connect first from the main window.");
        return;
    }

    PatmStrBuf json = {};
    PatmError err = patm_db_list_tables(conn, &json);
    if (!patm_is_ok(&err)) {
        m_status->setText("Failed to list tables: " + QString(err.msg));
        return;
    }

    if (json.data) {
        const char *p = json.data;
        char name[256];
        while ((p = strchr(p, '"')) != nullptr) {
            p++;
            const char *end = strchr(p, '"');
            if (!end) break;
            size_t len = (size_t)(end - p);
            if (len >= sizeof(name)) len = sizeof(name) - 1;
            memcpy(name, p, len);
            name[len] = '\0';
            m_tableCombo->addItem(QString::fromUtf8(name));
            p = end + 1;
        }
    }
    patm_strbuf_free(&json);

    if (m_tableCombo->count() == 0)
        m_status->setText("No tables found.");
    else
        m_status->setText(QString("Loaded %1 table(s).").arg(m_tableCombo->count()));
}

void ToolRunner::onBrowseOutput()
{
    QString path = QFileDialog::getSaveFileName(this, "Choose output CSV file",
        m_paramEntries[1] ? m_paramEntries[1]->text() : "export.csv",
        "CSV files (*.csv)");
    if (!path.isEmpty() && m_paramEntries[1])
        m_paramEntries[1]->setText(path);
}

void ToolRunner::onRun()
{
    int toolSel = m_toolCombo->currentIndex();
    if (toolSel < 0 || toolSel >= m_toolPaths.size()) { m_status->setText("Pick a tool first."); return; }
    int srcSel = m_sourceCombo->currentIndex();
    if (srcSel < 0 || (size_t)srcSel >= m_cfg->nconns) { m_status->setText("Pick a source connection."); return; }

    QString toolBn = baseName(m_toolPaths[toolSel]);
    const ToolMeta *meta = nullptr;
    for (const auto &t : m_knownTools) { if (t.name == toolBn) { meta = &t; break; } }
    if (!meta) { m_status->setText("Unknown tool."); return; }

    bool isExportCSV = !meta->params.isEmpty() && meta->params[0].key == "table";
    bool isTransferTable = !meta->params.isEmpty() && meta->params[0].key == "source_table";

    QString json = "{";
    for (int i = 0; i < meta->params.size() && i < 8; i++) {
        if (i > 0) json += ',';
        QString val;
        if ((isExportCSV || isTransferTable) && i == 0 && m_tableCombo) {
            int tblSel = m_tableCombo->currentIndex();
            if (tblSel >= 0) val = m_tableCombo->currentText();
        } else if (m_paramEntries[i]) {
            val = m_paramEntries[i]->text();
        }
        json += QString("\"%1\":\"%2\"").arg(meta->params[i].key, val);
    }
    json += '}';

    PatmConn *source = MainWindow::activeConnection();
    if (!source) {
        m_status->setText("Not connected. Connect first from the main window.");
        return;
    }

    PatmConn *target = nullptr;
    PatmError err = patm_ok();
    if (!m_tgtHost->text().isEmpty()) {
        int engineIdx = m_tgtEngine->currentIndex();
        int engine = engineIdx == 1 ? PATM_DB_MYSQL : engineIdx == 2 ? PATM_DB_MARIADB : PATM_DB_POSTGRESQL;
        err = connectWithParams(m_tgtHost->text().toUtf8().constData(),
            m_tgtPort->text().toInt(), m_tgtUser->text().toUtf8().constData(),
            m_tgtPass->text().toUtf8().constData(), m_tgtDb->text().toUtf8().constData(),
            engine, PATM_SSL_DISABLE, &target);
        if (!patm_is_ok(&err)) {
            appendLog("target connect failed: " + QString(err.msg) + "\n");
            m_status->setText("Target connection failed.");
            return;
        }
    }

    m_logView->clear();
    appendLog("=== running " + baseName(m_toolPaths[toolSel]) + " ===\n");

    PatmStrBuf log_out;
    patm_strbuf_init(&log_out);
    PatmPipelineCtx ctxp = {source, target};
    QByteArray jsonBytes = json.toUtf8();
    err = patm_pipeline_run_tool(m_toolPaths[toolSel].toUtf8().constData(), &ctxp,
                                 jsonBytes.constData(), &log_out);

    if (log_out.data && log_out.len)
        appendLog(QString::fromUtf8(log_out.data, log_out.len));
    else
        appendLog("(no output)\n");
    patm_strbuf_free(&log_out);

    if (target) patm_db_close(target);

    if (patm_is_ok(&err)) {
        appendLog("\n=== finished OK ===\n");
        m_status->setText("Done.");
    } else {
        appendLog("\n=== FAILED ===\n");
        if (err.msg[0])
            appendLog("Error: " + QString(err.msg) + "\n");
        m_status->setText("Failed: " + QString(err.msg));
    }
}

void ToolRunner::onEditTool()
{
    int sel = m_toolCombo->currentIndex();
    if (sel < 0 || sel >= m_toolPaths.size()) { m_status->setText("Pick a tool to edit."); return; }

    ToolEditor *editor = new ToolEditor(m_toolPaths[sel], MainWindow::instance());
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->show();
}

void ToolRunner::appendLog(const QString &text)
{
    m_logView->moveCursor(QTextCursor::End);
    m_logView->insertPlainText(text);
    QScrollBar *sb = m_logView->verticalScrollBar();
    sb->setValue(sb->maximum());
}

QString ToolRunner::sourceConnectionId() const
{
    int idx = m_sourceCombo->currentIndex();
    if (idx < 0 || (size_t)idx >= m_cfg->nconns) return {};
    return QString::fromUtf8(m_cfg->conns[idx].id);
}

QString ToolRunner::toolName() const
{
    int idx = m_toolCombo->currentIndex();
    if (idx < 0 || idx >= m_toolPaths.size()) return {};
    return baseName(m_toolPaths[idx]);
}

void ToolRunner::restoreState(const QString &toolName, const QString &connId)
{
    for (int i = 0; i < m_toolPaths.size(); i++) {
        if (baseName(m_toolPaths[i]) == toolName) {
            m_toolCombo->blockSignals(true);
            m_toolCombo->setCurrentIndex(i);
            m_toolCombo->blockSignals(false);
            onToolChanged(i);
            break;
        }
    }
    for (int i = 0; i < m_sourceCombo->count(); i++) {
        if (m_sourceCombo->itemText(i) == connId) {
            m_sourceCombo->blockSignals(true);
            m_sourceCombo->setCurrentIndex(i);
            m_sourceCombo->blockSignals(false);
            onSourceChanged(i);
            break;
        }
    }
}
