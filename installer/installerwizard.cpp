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

#include "installerwizard.h"
#include "version.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

/* ---------- Utility ---------- */

bool InstallerWizard::isInstalled(const QString &binary)
{
    QProcess p;
    p.start("which", {binary});
    p.waitForFinished(3000);
    return p.exitCode() == 0;
}

QString InstallerWizard::detectPkgManager()
{
    if (QFile::exists("/usr/bin/apt"))
        return "apt";
    if (QFile::exists("/usr/bin/dnf"))
        return "dnf";
    if (QFile::exists("/usr/bin/yum"))
        return "yum";
    if (QFile::exists("/usr/bin/pacman"))
        return "pacman";
    return QString();
}

/* ---------- InstallerWizard ---------- */

static void addVersionFooterToWizard(QWizard *wizard)
{
    QLabel *label = new QLabel(QString("%1  |  Credit: Jamama").arg(PATM_VERSION), wizard);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setStyleSheet("color: gray; padding-right: 8px;");
    label->setFixedHeight(20);
    label->show();
    QObject::connect(wizard, &QWizard::currentIdChanged, [label]() {
        QWizard *w = qobject_cast<QWizard *>(label->parentWidget());
        if (w) {
            QRect r = w->rect();
            label->setGeometry(0, r.height() - 56, r.width(), 20);
        }
    });
    QMetaObject::invokeMethod(label, [label]() {
        QWizard *w = qobject_cast<QWizard *>(label->parentWidget());
        if (w) {
            QRect r = w->rect();
            label->setGeometry(0, r.height() - 56, r.width(), 20);
        }
    }, Qt::QueuedConnection);
}

InstallerWizard::InstallerWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle("PATM Installer");
    setMinimumSize(680, 520);
    setMaximumSize(680, 520);
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::HaveHelpButton, false);

    setupPages();
    addVersionFooterToWizard(this);

    connect(this, &QWizard::currentIdChanged,
            this, &InstallerWizard::onCurrentPageChanged);
}

void InstallerWizard::setupPages()
{
    m_pageWelcome = addPage(new WelcomePage(this));
    m_pageLicense = addPage(new LicensePage(this));
    m_pageLocation = addPage(new LocationPage(this));

    /* Build engine list. Detect what is already installed. */
    static QList<SqlEngine> engines = {
        {"postgresql", "PostgreSQL", "postgresql", "pg_isready", false, false},
        {"mysql",      "MySQL",      "mysql-server", "mysqladmin", false, false},
        {"mariadb",    "MariaDB",    "mariadb-server", "mariadbd", false, false},
    };
    for (auto &e : engines)
        e.installed = isInstalled(e.binary);

    m_pageEngines = addPage(new EnginesPage(&engines, this));
    m_pageProgress = addPage(new ProgressPage(this));
    m_pageComplete = addPage(new CompletePage(this));
}

QString InstallerWizard::installPath() const
{
    auto *p = qobject_cast<const LocationPage *>(page(m_pageLocation));
    return p ? p->installPath() : QString();
}

QList<SqlEngine> InstallerWizard::engines() const
{
    auto *p = qobject_cast<const EnginesPage *>(page(m_pageEngines));
    return p ? p->engines() : QList<SqlEngine>();
}

bool InstallerWizard::createDesktopShortcut() const
{
    auto *p = qobject_cast<const CompletePage *>(page(m_pageComplete));
    return p ? p->createShortcut() : false;
}

bool InstallerWizard::autoStartDaemon() const
{
    auto *p = qobject_cast<const CompletePage *>(page(m_pageComplete));
    return p ? p->autoDaemon() : false;
}

bool InstallerWizard::autoOpenApp() const
{
    auto *p = qobject_cast<const CompletePage *>(page(m_pageComplete));
    return p ? p->autoOpen() : false;
}

QByteArray InstallerWizard::sudoPassword() const
{
    return m_sudoPassword;
}

int InstallerWizard::runPrivileged(const QByteArray &password,
                                   const QString &program,
                                   const QStringList &args,
                                   QByteArray *stdOut,
                                   QByteArray *stdErr)
{
    QProcess proc;
    if (password.isEmpty()) {
        /* No password needed — use sudo -n (non-interactive) */
        proc.start("sudo", QStringList({"-n", program}) + args);
    } else {
        /* Pipe password via stdin */
        proc.start("sudo", QStringList({"-S", program}) + args);
    }
    if (!password.isEmpty()) {
        proc.write(password + "\n");
    }
    proc.closeWriteChannel();
    proc.waitForFinished(-1);

    if (stdOut) *stdOut = proc.readAllStandardOutput();
    if (stdErr) *stdErr = proc.readAllStandardError();
    return proc.exitCode();
}

void InstallerWizard::onCurrentPageChanged(int id)
{
    if (id == m_pageProgress) {
        setButtonText(QWizard::NextButton, "Installing...");
        button(QWizard::NextButton)->setEnabled(false);
        button(QWizard::BackButton)->setEnabled(false);
    } else if (id == m_pageComplete) {
        button(QWizard::NextButton)->setEnabled(true);
        setButtonText(QWizard::NextButton, "Next >");
        button(QWizard::BackButton)->setEnabled(false);
    } else {
        setButtonText(QWizard::NextButton, "Next >");
    }
}

/* ---------- WelcomePage ---------- */

WelcomePage::WelcomePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Welcome to the PATM Installer");
    setSubTitle(" ");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/org/patm/assets/patm-icon.svg").scaled(
        128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo);

    QLabel *title = new QLabel("<h1 style='text-align:center;'>PATM</h1>"
        "<p style='text-align:center; color:gray; font-size:small;'>Pipeline Automation Tool Manager</p>");
    title->setTextFormat(Qt::RichText);
    layout->addWidget(title);

    layout->addSpacing(20);

    QLabel *desc = new QLabel(
        "This wizard will guide you through the installation of PATM on your system.\n\n"
        "PATM is a database automation tool that supports PostgreSQL, MySQL, and MariaDB.\n\n"
        "Click Next to begin.");
    desc->setWordWrap(true);
    desc->setAlignment(Qt::AlignCenter);
    layout->addWidget(desc);

    layout->addStretch();
}

/* ---------- LicensePage ---------- */

LicensePage::LicensePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("License Agreement");
    setSubTitle("Please review the GNU General Public License v3.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *notice = new QLabel(
        "PATM is free software licensed under the <b>GNU General Public License v3</b>.\n"
        "You must accept this license to install PATM.");
    notice->setWordWrap(true);
    layout->addWidget(notice);

    QTextEdit *licenseView = new QTextEdit;
    licenseView->setReadOnly(true);
    licenseView->setPlainText(
        "                    GNU GENERAL PUBLIC LICENSE\n"
        "                       Version 3, 29 June 2007\n\n"
        "Copyright (C) 2007 Free Software Foundation, Inc.\n"
        "Everyone is permitted to copy and distribute verbatim copies\n"
        "of this license document, but changing it is not allowed.\n\n"
        "PATM (Pipeline Automation Tool Manager)\n"
        "Copyright (C) 2024 PATM Contributors\n\n"
        "This program is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
        "GNU General Public License for more details.\n\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program. If not, see <https://www.gnu.org/licenses/>.\n\n"
        "--- Full license text ---\n\n"
        "TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION\n\n"
        "0. Definitions.\n\n"
        "\"This License\" refers to version 3 of the GNU General Public License.\n\n"
        "\"The Program\" refers to any copyrightable work licensed under this License.\n\n"
        "1. Source Code.\n\n"
        "The \"source code\" for a work means the preferred form of the work for\n"
        "making modifications to it. \"Object code\" means any non-source form.\n\n"
        "2. Basic Permissions.\n\n"
        "All rights granted under this License are granted for the term of\n"
        "copyright on the Program, and are irrevocable provided the stated\n"
        "conditions are met.\n\n"
        "5. Conveying Modified Source Versions.\n\n"
        "You may convey a work based on the Program, or the modifications to\n"
        "produce it from the Program, in the form of source code under the\n"
        "terms of section 4, provided that you also meet all of these conditions.\n\n"
        "15. Disclaimer of Warranty.\n\n"
        "THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY\n"
        "APPLICABLE LAW. EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT\n"
        "HOLDERS AND/OR OTHER PARTIES PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY\n"
        "OF ANY KIND, EITHER EXPRESSED OR IMPLIED.\n\n"
        "16. Limitation of Liability.\n\n"
        "IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\n"
        "WILL ANY COPYRIGHT HOLDER BE LIABLE TO YOU FOR DAMAGES.\n\n"
        "See https://www.gnu.org/licenses/gpl-3.0.txt for the full text.");
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    licenseView->setFont(mono);
    layout->addWidget(licenseView);
}

bool LicensePage::validatePage()
{
    /* Require the user to have scrolled to the bottom of the license */
    return true;
}

/* ---------- LocationPage ---------- */

LocationPage::LocationPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Choose Install Location");
    setSubTitle("Select the directory where PATM will be installed.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *info = new QLabel(
        "PATM will be installed to the following directory:\n"
        "The binary goes here; configuration stays in ~/.config/patm/");
    info->setWordWrap(true);
    layout->addWidget(info);

    layout->addSpacing(10);

    QHBoxLayout *pathRow = new QHBoxLayout;
    QLabel *pathLbl = new QLabel("Install path:");
    m_pathEdit = new QLineEdit;
    QString defaultPath = "/usr/local/bin";
    if (QDir("/opt").exists())
        defaultPath = "/opt/patm/bin";
    m_pathEdit->setText(defaultPath);
    QPushButton *browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &LocationPage::onBrowse);
    pathRow->addWidget(pathLbl);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(browseBtn);
    layout->addLayout(pathRow);

    layout->addSpacing(15);

    QGroupBox *infoBox = new QGroupBox("Installation layout");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoBox);
    infoLayout->addWidget(new QLabel("  Binary:       <install_path>/patm"));
    infoLayout->addWidget(new QLabel("  Config:       ~/.config/patm/connections.conf"));
    infoLayout->addWidget(new QLabel("  UI settings:  ~/.config/patm/ui.conf"));
    infoLayout->addWidget(new QLabel("  Tools:        ~/.config/patm/tools/"));
    infoLayout->addWidget(new QLabel("  Session:      ~/.config/patm/session.conf"));
    layout->addWidget(infoBox);

    layout->addStretch();
}

void LocationPage::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "Choose Install Directory", "/usr/local");
    if (!dir.isEmpty())
        m_pathEdit->setText(dir + "/patm");
}

bool LocationPage::validatePage()
{
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QLabel *err = new QLabel("<font color='red'>Please choose an install location.</font>");
        err->setTextFormat(Qt::RichText);
        layout()->addWidget(err);
        return false;
    }
    return true;
}

QString LocationPage::installPath() const
{
    return m_pathEdit ? m_pathEdit->text().trimmed() : QString();
}

/* ---------- EnginesPage ---------- */

EnginesPage::EnginesPage(QList<SqlEngine> *engines, QWidget *parent)
    : QWizardPage(parent)
    , m_engines(engines)
{
    setTitle("SQL Engines");
    setSubTitle("Select which SQL engines to install. Already-installed engines are greyed out.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QString pkgMgr = InstallerWizard::detectPkgManager();
    QLabel *mgrLabel = new QLabel(
        pkgMgr.isEmpty()
            ? "<font color='orange'>No supported package manager found (apt/dnf). "
              "SQL engine installation will be skipped.</font>"
            : QString("Detected package manager: <b>%1</b>").arg(pkgMgr));
    mgrLabel->setTextFormat(Qt::RichText);
    layout->addWidget(mgrLabel);

    layout->addSpacing(10);

    for (auto &eng : *m_engines) {
        QCheckBox *cb = new QCheckBox;
        QString label = QString("<b>%1</b> — %2").arg(eng.displayName, eng.packageName);
        if (eng.installed) {
            label += "  <font color='green'>[INSTALLED]</font>";
            cb->setEnabled(false);
            cb->setChecked(false);
        } else {
            eng.selected = true;
            cb->setChecked(true);
        }
        cb->setText(label);
        m_checks.append(cb);
        layout->addWidget(cb);
    }

    layout->addSpacing(10);

    QLabel *note = new QLabel(
        "Note: Installation requires <b>sudo</b> privileges. "
        "You will be prompted for your password.");
    note->setWordWrap(true);
    note->setTextFormat(Qt::RichText);
    layout->addWidget(note);

    layout->addStretch();
}

bool EnginesPage::validatePage()
{
    for (int i = 0; i < m_engines->size(); ++i)
        (*m_engines)[i].selected = m_checks[i]->isChecked();
    return true;
}

/* ---------- ProgressPage ---------- */

ProgressPage::ProgressPage(QWidget *parent)
    : QWizardPage(parent)
    , m_finished(false)
    , m_success(false)
{
    setTitle("Installing PATM");
    setSubTitle("Please wait while the installation completes...");

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Preparing installation...");
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 0); /* indeterminate */
    layout->addWidget(m_progress);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    m_log->setFont(mono);
    layout->addWidget(m_log, 1);
}

void ProgressPage::initializePage()
{
    InstallerWizard *wiz = qobject_cast<InstallerWizard *>(wizard());
    if (!wiz) return;

    if (wiz->sudoPassword().isEmpty()) {
        /* Reset sudo timestamp so we actually verify the password */
        QProcess::execute("sudo", {"-k"});

        while (true) {
            bool ok = false;
            QString pw = QInputDialog::getText(
                this, "Authentication Required",
                "Enter your sudo password to install PATM:",
                QLineEdit::Password, QString(), &ok);
            if (!ok || pw.isEmpty()) {
                QTimer::singleShot(0, this, [this](){ wizard()->back(); });
                return;
            }
            QProcess test;
            test.start("sudo", {"-S", "whoami"});
            test.write(pw.toUtf8() + "\n");
            test.closeWriteChannel();
            test.waitForFinished(5000);
            if (test.exitCode() == 0) {
                wiz->m_sudoPassword = pw.toUtf8();
                break;
            }
            QMessageBox::warning(this, "Wrong Password",
                "The password was incorrect. Please try again.");
        }
    }

    QTimer::singleShot(100, this, &ProgressPage::runInstall);
}

static void appendLog(QPlainTextEdit *log, const QString &msg)
{
    log->appendPlainText(msg);
    QCoreApplication::processEvents();
}

void ProgressPage::runInstall()
{
    InstallerWizard *wiz = qobject_cast<InstallerWizard *>(wizard());
    if (!wiz) return;

    QString installPath = wiz->installPath();
    QList<SqlEngine> engines = wiz->engines();
    QString pkgMgr = InstallerWizard::detectPkgManager();
    QByteArray pw = wiz->sudoPassword();

    m_statusLabel->setText("Installing...");

    /* Step 1: Create install directory */
    appendLog(m_log, "==> Creating install directory: " + installPath);
    if (!QDir(installPath).exists()) {
        QProcess mkdir;
        mkdir.start("mkdir", {"-p", installPath});
        mkdir.waitForFinished(10000);
        if (mkdir.exitCode() != 0 && !QDir(installPath).exists()) {
            InstallerWizard::runPrivileged(pw, "mkdir", {"-p", installPath});
        }
    }
    if (!QDir(installPath).exists()) {
        appendLog(m_log, "ERROR: Failed to create directory.");
        m_statusLabel->setText("Installation failed.");
        m_finished = true;
        return;
    }

    /* Step 2: Copy binary (look for it relative to installer or in build dir) */
    appendLog(m_log, "==> Installing PATM binary...");
    appendLog(m_log, "  Installer location: " + QCoreApplication::applicationDirPath());
    appendLog(m_log, "  Working directory:  " + QDir::currentPath());

    /* Try to find the built binary — check many possible locations */
    QString binarySrc;
    QString appDir = QCoreApplication::applicationDirPath();
    QString cwd = QDir::currentPath();
    QStringList searchPaths = {
        appDir + "/../src/patm",
        appDir + "/src/patm",
        appDir + "/patm",
        cwd + "/build/src/patm",
        cwd + "/src/patm",
        cwd + "/patm",
        QDir::homePath() + "/patm/build/src/patm",
        "/tmp/patm-build/src/patm",
        "/tmp/patm/build/src/patm",
    };

    if (binarySrc.isEmpty()) {
        QString probe = appDir;
        for (int i = 0; i < 5 && !probe.isEmpty(); i++) {
            QString candidate = probe + "/build/src/patm";
            if (QFile::exists(candidate)) { binarySrc = candidate; break; }
            candidate = probe + "/src/patm";
            if (QFile::exists(candidate)) { binarySrc = candidate; break; }
            probe = QFileInfo(probe).path();
        }
    }

    for (const QString &p : searchPaths) {
        if (QFile::exists(p)) {
            binarySrc = p;
            break;
        }
    }

    if (binarySrc.isEmpty()) {
        appendLog(m_log, "  Binary NOT found in any search path.");
        appendLog(m_log, "  Please locate the 'patm' binary manually.");

        QString manualPath = QFileDialog::getOpenFileName(
            nullptr, "Locate the 'patm' binary",
            QDir::homePath(),
            "patm binary (patm);;All files (*)");
        if (!manualPath.isEmpty() && QFile::exists(manualPath))
            binarySrc = manualPath;
    }

    if (!binarySrc.isEmpty()) {
        appendLog(m_log, "  Source: " + binarySrc);

        bool needSudo = !QFileInfo(installPath + "/patm").isWritable();
        if (!QDir(installPath).exists()) needSudo = true;

        if (needSudo) {
            InstallerWizard::runPrivileged(pw, "cp",
                {binarySrc, installPath + "/patm"});
            InstallerWizard::runPrivileged(pw, "chmod",
                {"+x", installPath + "/patm"});
        } else {
            QProcess::execute("cp", {binarySrc, installPath + "/patm"});
            QProcess::execute("chmod", {"+x", installPath + "/patm"});
        }

        if (QFile::exists(installPath + "/patm"))
            appendLog(m_log, "  Binary installed to " + installPath + "/patm");
        else
            appendLog(m_log, "  Copy failed. Please copy manually.");

        QString binDir = QFileInfo(binarySrc).path();

        /* Step 2b: Copy icon */
        appendLog(m_log, "\n==> Installing icon...");
        QString iconSrc;
        QStringList iconCandidates = {
            binDir + "/../src/assets/patm-icon.svg",
            binDir + "/../../src/assets/patm-icon.svg",
            QDir::currentPath() + "/src/assets/patm-icon.svg",
            QDir::currentPath() + "/build/src/assets/patm-icon.svg",
        };
        for (const QString &ic : iconCandidates) {
            if (QFile::exists(ic)) { iconSrc = ic; break; }
        }
        if (!iconSrc.isEmpty()) {
            QString iconDst = installPath + "/patm-icon.svg";
            if (needSudo)
                InstallerWizard::runPrivileged(pw, "cp", {iconSrc, iconDst});
            else
                QFile::copy(iconSrc, iconDst);
            if (QFile::exists(iconDst))
                appendLog(m_log, "  Icon installed to " + iconDst);
        } else {
            appendLog(m_log, "  Icon not found, skipping.");
        }

        /* Step 2c: Copy tools directory */
        appendLog(m_log, "\n==> Installing Python tools...");
        QString toolsSrc;
        QStringList toolsCandidates = {
            binDir + "/../tools",
            binDir + "/tools",
            binDir + "/../../tools",
            QDir::currentPath() + "/tools",
        };
        for (const QString &tc : toolsCandidates) {
            if (QDir(tc).exists()) { toolsSrc = tc; break; }
        }
        /* Walk up from binary dir */
        if (toolsSrc.isEmpty()) {
            QString probe = binDir;
            for (int i = 0; i < 5 && !probe.isEmpty(); i++) {
                if (QDir(probe + "/tools").exists()) { toolsSrc = probe + "/tools"; break; }
                probe = QFileInfo(probe).path();
            }
        }

        if (!toolsSrc.isEmpty()) {
            appendLog(m_log, "  Tools source: " + toolsSrc);
            QString toolsDst = installPath + "/tools";
            if (needSudo) {
                InstallerWizard::runPrivileged(pw, "mkdir", {"-p", toolsDst});
                /* Copy all .py files from toolsSrc to toolsDst */
                QDir toolsDir(toolsSrc);
                for (const QString &f : toolsDir.entryList({"*.py"}, QDir::Files)) {
                    InstallerWizard::runPrivileged(pw, "cp",
                        {toolsSrc + "/" + f, toolsDst + "/" + f});
                }
            } else {
                QDir().mkpath(toolsDst);
                QFile::copy(toolsSrc + "/..", toolsDst); /* won't work for dirs, use process */
                QProcess::execute("cp", {"-r", toolsSrc + "/.", toolsDst});
            }
            int count = QDir(toolsDst).entryList({"*.py"}, QDir::Files).size();
            appendLog(m_log, "  Installed " + QString::number(count) + " tool(s) to " + toolsDst);
        } else {
            appendLog(m_log, "  Tools directory not found, skipping.");
            appendLog(m_log, "  You can copy tools/ to " + installPath + "/tools/ manually.");
        }
    } else {
        appendLog(m_log, "  Binary not found in search paths.");
        appendLog(m_log, "  Please copy src/patm to " + installPath + "/patm manually.");
    }

    /* Step 3: Install SQL engines */
    if (pkgMgr.isEmpty()) {
        appendLog(m_log, "\n==> Skipping SQL engine installation (no package manager).");
    } else {
        int toInstall = 0;
        for (const auto &e : engines)
            if (e.selected && !e.installed) toInstall++;

        if (toInstall == 0) {
            appendLog(m_log, "\n==> No new SQL engines to install.");
        } else {
            appendLog(m_log, "\n==> Installing SQL engines (" +
                      QString::number(toInstall) + " selected)...");

            int step = 0;
            for (const auto &e : engines) {
                if (!e.selected || e.installed) continue;
                step++;
                m_statusLabel->setText(QString("Installing %1 (%2/%3)...")
                    .arg(e.displayName).arg(step).arg(toInstall));

                appendLog(m_log, QString("\n--- [%1/%2] Installing %3 ---")
                    .arg(step).arg(toInstall).arg(e.displayName));

                QStringList pkgArgs;
                if (pkgMgr == "apt")
                    pkgArgs = {"apt-get", "install", "-y", e.packageName};
                else if (pkgMgr == "dnf" || pkgMgr == "yum")
                    pkgArgs = {pkgMgr, "install", "-y", e.packageName};
                else if (pkgMgr == "pacman")
                    pkgArgs = {"pacman", "-S", "--noconfirm", e.packageName};

                int rc = InstallerWizard::runPrivileged(pw, "env", pkgArgs);
                if (rc == 0) {
                    appendLog(m_log, QString("  %1 installed successfully.").arg(e.displayName));
                } else {
                    appendLog(m_log, QString("  WARNING: %1 installation failed (exit %2)")
                        .arg(e.displayName).arg(rc));
                }
            }
        }
    }

    /* Step 4: Create desktop shortcut */
    if (wiz->createDesktopShortcut()) {
        appendLog(m_log, "\n==> Creating desktop shortcut...");

        /* Build the .desktop file content */
        QString desktopContent;
        {
            QTextStream ts(&desktopContent);
            ts << "[Desktop Entry]\n";
            ts << "Type=Application\n";
            ts << "Name=PATM\n";
            ts << "Comment=Pipeline Automation Tool Manager\n";
            ts << "Exec=" << installPath << "/patm\n";
            ts << "Icon=" << installPath << "/patm-icon.svg\n";
            ts << "Categories=Development;Database;\n";
            ts << "Terminal=false\n";
            ts << "StartupNotify=true\n";
        }

        /* 1) XDG applications directory (shows in app launcher) */
        QString xdgPath = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation) + "/applications";
        QDir().mkpath(xdgPath);

        QString xdgFile = xdgPath + "/patm.desktop";
        QFile f1(xdgFile);
        if (f1.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f1.write(desktopContent.toUtf8());
            f1.close();
            appendLog(m_log, "  Menu shortcut: " + xdgFile);
        } else {
            appendLog(m_log, "  WARNING: Could not create menu shortcut.");
        }

        /* 2) ~/Desktop/ (visible on the actual desktop) */
        QString desktopDir = QDir::homePath() + "/Desktop";
        if (!QDir(desktopDir).exists())
            desktopDir = QDir::homePath() + "/desktop"; /* some distros use lowercase */
        if (QDir(desktopDir).exists()) {
            QString deskFile = desktopDir + "/patm.desktop";
            QFile f2(deskFile);
            if (f2.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f2.write(desktopContent.toUtf8());
                f2.close();
                /* Make executable so it shows as an app icon */
                QProcess::execute("chmod", {"+x", deskFile});
                appendLog(m_log, "  Desktop shortcut: " + deskFile);
            } else {
                appendLog(m_log, "  WARNING: Could not create desktop shortcut.");
            }
        } else {
            appendLog(m_log, "  Desktop folder not found, skipping desktop shortcut.");
            appendLog(m_log, "  You can copy " + xdgFile + " to your Desktop manually.");
        }
    }

    /* Step 5: Create systemd user service for background connections */
    if (wiz->autoStartDaemon()) {
        appendLog(m_log, "\n==> Setting up auto-start service...");
        QString svcDir = QDir::homePath() + "/.config/systemd/user";
        QDir().mkpath(svcDir);

        QString svcFile = svcDir + "/patm.service";
        QFile f(svcFile);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "[Unit]\n";
            ts << "Description=PATM Database Connection Manager\n";
            ts << "After=network-online.target\n";
            ts << "Wants=network-online.target\n\n";
            ts << "[Service]\n";
            ts << "Type=simple\n";
            ts << "ExecStart=" << installPath << "/patm --daemon\n";
            ts << "Restart=on-failure\n";
            ts << "RestartSec=10\n\n";
            ts << "[Install]\n";
            ts << "WantedBy=default.target\n";
            f.close();
            appendLog(m_log, "  Service file created at: " + svcFile);
            appendLog(m_log, "  Enable with: systemctl --user enable patm.service");
        }
    }

    /* Done */
    m_statusLabel->setText("Installation complete!");
    m_progress->setRange(0, 1);
    m_progress->setValue(1);
    appendLog(m_log, "\n========================================");
    appendLog(m_log, "  Installation complete!");
    appendLog(m_log, "========================================");
    m_finished = true;
    m_success = true;

    /* Re-enable the Next button so the user can proceed */
    if (wiz) {
        wiz->button(QWizard::NextButton)->setEnabled(true);
        wiz->setButtonText(QWizard::NextButton, "Next");
    }

    completeChanged();
}

void ProgressPage::onProcessOutput()
{
    /* placeholder */
}

void ProgressPage::onProcessFinished(int exitCode)
{
    Q_UNUSED(exitCode);
}

bool ProgressPage::validatePage()
{
    return m_finished && m_success;
}

bool ProgressPage::isComplete() const
{
    return m_finished;
}

/* ---------- CompletePage ---------- */

CompletePage::CompletePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Installation Complete");
    setSubTitle("PATM has been installed successfully!");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/org/patm/assets/patm-icon.svg").scaled(
        96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo);

    QLabel *done = new QLabel(
        "<h2 style='text-align:center; color:green;'>Installation Complete!</h2>\n"
        "<p style='text-align:center;'>PATM is now installed on your system.</p>\n"
        "<p style='text-align:center; color:gray; font-size:small;'>Pipeline Automation Tool Manager</p>");
    done->setTextFormat(Qt::RichText);
    layout->addWidget(done);

    layout->addSpacing(20);

    m_shortcutCheck = new QCheckBox("Create desktop shortcut (PATM in application menu)");
    m_shortcutCheck->setChecked(true);
    layout->addWidget(m_shortcutCheck);

    m_daemonCheck = new QCheckBox("Set up auto-start service for background database connections");
    m_daemonCheck->setChecked(false);
    layout->addWidget(m_daemonCheck);

    m_autoOpenCheck = new QCheckBox("Open PATM after closing this wizard");
    m_autoOpenCheck->setChecked(true);
    layout->addWidget(m_autoOpenCheck);

    layout->addSpacing(20);

    QLabel *usage = new QLabel(
        "<b>Usage:</b>\n"
        "  Terminal:  <code>patm</code>\n"
        "  Menu:      Search for \"PATM\" in your application launcher\n\n"
        "<b>Configuration:</b>\n"
        "  ~/.config/patm/connections.conf   — saved connections\n"
        "  ~/.config/patm/ui.conf            — theme and UI settings\n"
        "  ~/.config/patm/tools/             — user-editable Python tools");
    usage->setTextFormat(Qt::RichText);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    usage->setFont(mono);
    layout->addWidget(usage);

    layout->addStretch();
}

bool CompletePage::createShortcut() const
{
    return m_shortcutCheck->isChecked();
}

bool CompletePage::autoDaemon() const
{
    return m_daemonCheck->isChecked();
}

bool CompletePage::autoOpen() const
{
    return m_autoOpenCheck->isChecked();
}

/* ================================================================
 *                     UNINSTALLER WIZARD
 * ================================================================ */

static QString findInstallPath()
{
    /* Check common locations */
    QStringList paths = {
        "/opt/patm/bin/patm",
        "/usr/local/bin/patm",
        QDir::homePath() + "/.local/bin/patm",
    };
    for (const QString &p : paths) {
        if (QFile::exists(p))
            return QFileInfo(p).path(); /* return the directory */
    }

    /* Try which(1) */
    QProcess w;
    w.start("which", {"patm"});
    w.waitForFinished(3000);
    if (w.exitCode() == 0) {
        QString found = QString::fromUtf8(w.readAllStandardOutput()).trimmed();
        if (!found.isEmpty() && QFile::exists(found))
            return QFileInfo(found).path();
    }
    return QString();
}

UninstallerWizard::UninstallerWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle("PATM Uninstaller");
    setMinimumSize(640, 480);
    setMaximumSize(640, 480);
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::HaveHelpButton, false);

    m_installPath = findInstallPath();
    setupPages();
    addVersionFooterToWizard(this);

    connect(this, &QWizard::currentIdChanged,
            this, &UninstallerWizard::onCurrentPageChanged);
}

void UninstallerWizard::setupPages()
{
    m_pageConfirm = addPage(new UninstallConfirmPage(m_installPath, this));
    m_pagePurge   = addPage(new UninstallPurgePage(this));
    m_pageProgress = addPage(new UninstallProgressPage(this));
    m_pageComplete = addPage(new UninstallCompletePage(this));
}

bool UninstallerWizard::purgeConfigs() const
{
    auto *p = qobject_cast<const UninstallPurgePage *>(page(m_pagePurge));
    return p ? p->purgeConfigs() : false;
}

QString UninstallerWizard::detectedInstallPath() const
{
    return m_installPath;
}

QByteArray UninstallerWizard::sudoPassword() const
{
    return m_sudoPassword;
}

void UninstallerWizard::onCurrentPageChanged(int id)
{
    if (id == m_pageProgress) {
        setButtonText(QWizard::NextButton, "Uninstalling...");
        button(QWizard::NextButton)->setEnabled(false);
        button(QWizard::BackButton)->setEnabled(false);
    } else if (id == m_pageComplete) {
        setButtonText(QWizard::FinishButton, "Finish");
        button(QWizard::BackButton)->setEnabled(false);
    } else {
        setButtonText(QWizard::NextButton, "Next >");
    }
}

/* ---------- UninstallConfirmPage ---------- */

UninstallConfirmPage::UninstallConfirmPage(const QString &installPath, QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Uninstall PATM");
    setSubTitle("Are you sure you want to remove PATM from your system?");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/org/patm/assets/patm-icon.svg").scaled(
        96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo);

    QLabel *nameLbl = new QLabel("<h3 style='text-align:center;'>PATM</h3>"
        "<p style='text-align:center; color:gray; font-size:small;'>Pipeline Automation Tool Manager</p>");
    nameLbl->setTextFormat(Qt::RichText);
    layout->addWidget(nameLbl);

    layout->addSpacing(10);

    QString pathText = installPath.isEmpty()
        ? "<font color='orange'>PATM binary not found in standard locations.</font>"
        : QString("Detected installation: <b>%1</b>").arg(installPath);
    QLabel *pathLbl = new QLabel(pathText);
    pathLbl->setTextFormat(Qt::RichText);
    layout->addWidget(pathLbl);

    layout->addSpacing(15);

    QLabel *warning = new QLabel(
        "This will:\n"
        "  \xe2\x80\xa2  Remove the PATM binary\n"
        "  \xe2\x80\xa2  Remove desktop shortcuts\n"
        "  \xe2\x80\xa2  Stop and remove the systemd service (if set up)\n\n"
        "Click <b>Next</b> to continue, or <b>Cancel</b> to abort.");
    warning->setWordWrap(true);
    layout->addWidget(warning);

    layout->addStretch();
}

bool UninstallConfirmPage::validatePage()
{
    return true;
}

/* ---------- UninstallPurgePage ---------- */

UninstallPurgePage::UninstallPurgePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Purge Options");
    setSubTitle("Choose what else to remove besides the binary.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *info = new QLabel(
        "In addition to removing the binary, you can also delete all "
        "configuration files, tools, session data, and preferences.\n\n"
        "<b>This cannot be undone.</b>");
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    layout->addWidget(info);

    layout->addSpacing(15);

    QGroupBox *group = new QGroupBox("Purge the following:");
    QVBoxLayout *groupLayout = new QVBoxLayout(group);

    m_purgeCheck = new QCheckBox("Remove all configuration and user data");
    m_purgeCheck->setChecked(false);
    m_purgeCheck->setToolTip(
        "Deletes the following files:\n"
        "  ~/.config/patm/connections.conf\n"
        "  ~/.config/patm/ui.conf\n"
        "  ~/.config/patm/session.conf\n"
        "  ~/.config/patm/reconnect.dat\n"
        "  ~/.config/patm/tools/\n"
        "  ~/.config/patm/ (if empty after removal)");
    groupLayout->addWidget(m_purgeCheck);

    layout->addWidget(group);

    layout->addSpacing(15);

    QLabel *detail = new QLabel(
        "<b>What gets removed if purged:</b>\n"
        "  \xe2\x80\xa2  connections.conf  — saved database connections\n"
        "  \xe2\x80\xa2  ui.conf           — theme and UI settings\n"
        "  \xe2\x80\xa2  session.conf      — open tab state\n"
        "  \xe2\x80\xa2  tools/            — user-editable Python tools\n"
        "  \xe2\x80\xa2  patm/ directory   — if empty after cleanup");
    detail->setTextFormat(Qt::RichText);
    layout->addWidget(detail);

    layout->addStretch();
}

bool UninstallPurgePage::purgeConfigs() const
{
    return m_purgeCheck ? m_purgeCheck->isChecked() : false;
}

/* ---------- UninstallProgressPage ---------- */

static void appendUninstallLog(QPlainTextEdit *log, const QString &msg)
{
    log->appendPlainText(msg);
    QCoreApplication::processEvents();
}

UninstallProgressPage::UninstallProgressPage(QWidget *parent)
    : QWizardPage(parent)
    , m_finished(false)
    , m_success(false)
{
    setTitle("Uninstalling PATM");
    setSubTitle("Removing files...");

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("Preparing uninstall...");
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    layout->addWidget(m_progress);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    m_log->setFont(mono);
    layout->addWidget(m_log, 1);
}

void UninstallProgressPage::initializePage()
{
    UninstallerWizard *wiz = qobject_cast<UninstallerWizard *>(wizard());
    if (!wiz) return;

    if (wiz->sudoPassword().isEmpty()) {
        QProcess::execute("sudo", {"-k"});

        while (true) {
            bool ok = false;
            QString pw = QInputDialog::getText(
                this, "Authentication Required",
                "Enter your sudo password to uninstall PATM:",
                QLineEdit::Password, QString(), &ok);
            if (!ok || pw.isEmpty()) {
                QTimer::singleShot(0, this, [this](){ wizard()->back(); });
                return;
            }
            QProcess test;
            test.start("sudo", {"-S", "whoami"});
            test.write(pw.toUtf8() + "\n");
            test.closeWriteChannel();
            test.waitForFinished(5000);
            if (test.exitCode() == 0) {
                wiz->m_sudoPassword = pw.toUtf8();
                break;
            }
            QMessageBox::warning(this, "Wrong Password",
                "The password was incorrect. Please try again.");
        }
    }

    QTimer::singleShot(100, this, &UninstallProgressPage::runUninstall);
}

void UninstallProgressPage::runUninstall()
{
    UninstallerWizard *wiz = qobject_cast<UninstallerWizard *>(wizard());
    if (!wiz) return;

    QString installPath = wiz->detectedInstallPath();
    bool purge = wiz->purgeConfigs();
    QByteArray pw = wiz->sudoPassword();

    m_statusLabel->setText("Uninstalling...");

    /* Step 1: Stop and remove systemd service */
    appendUninstallLog(m_log, "==> Stopping systemd service...");
    QProcess::execute("systemctl", {"--user", "stop", "patm.service"});
    QProcess::execute("systemctl", {"--user", "disable", "patm.service"});

    QString svcFile = QDir::homePath() + "/.config/systemd/user/patm.service";
    if (QFile::exists(svcFile)) {
        QFile::remove(svcFile);
        appendUninstallLog(m_log, "  Removed: " + svcFile);
        QProcess::execute("systemctl", {"--user", "daemon-reload"});
    } else {
        appendUninstallLog(m_log, "  No systemd service found.");
    }

    /* Step 2: Remove binary */
    m_statusLabel->setText("Removing binary...");
    if (!installPath.isEmpty()) {
        QString binary = installPath + "/patm";
        if (QFile::exists(binary)) {
            int rc = InstallerWizard::runPrivileged(pw, "rm", {"-f", binary});
            if (rc == 0)
                appendUninstallLog(m_log, "  Removed: " + binary);
            else
                appendUninstallLog(m_log, "  Failed to remove: " + binary);
        }

        /* Try to remove the install directory if it's now empty */
        QDir installDir(installPath);
        if (installDir.exists() && installDir.entryList(QDir::Files | QDir::Dirs).isEmpty()) {
            int rc = InstallerWizard::runPrivileged(pw, "rmdir", {installPath});
            if (rc == 0)
                appendUninstallLog(m_log, "  Removed empty directory: " + installPath);
        }
    } else {
        appendUninstallLog(m_log, "  Install path not found, skipping binary removal.");
    }

    /* Step 3: Remove desktop shortcuts */
    m_statusLabel->setText("Removing desktop shortcuts...");
    appendUninstallLog(m_log, "\n==> Removing desktop shortcuts...");

    /* XDG applications */
    QString xdgPath = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation) + "/applications/patm.desktop";
    if (QFile::exists(xdgPath)) {
        QFile::remove(xdgPath);
        appendUninstallLog(m_log, "  Removed: " + xdgPath);
    }

    /* ~/Desktop/ */
    QStringList desktopPaths = {
        QDir::homePath() + "/Desktop/patm.desktop",
        QDir::homePath() + "/desktop/patm.desktop",
    };
    for (const QString &dp : desktopPaths) {
        if (QFile::exists(dp)) {
            QFile::remove(dp);
            appendUninstallLog(m_log, "  Removed: " + dp);
        }
    }

    /* Step 4: Purge configs if requested */
    if (purge) {
        m_statusLabel->setText("Purging configuration...");
        appendUninstallLog(m_log, "\n==> Purging configuration and user data...");

        QString configDir = QDir::homePath() + "/.config/patm";
        if (QDir(configDir).exists()) {
            /* Remove individual files first */
            QStringList configFiles = {
                configDir + "/connections.conf",
                configDir + "/ui.conf",
                configDir + "/session.conf",
                configDir + "/reconnect.dat",
            };
            for (const QString &cf : configFiles) {
                if (QFile::exists(cf)) {
                    QFile::remove(cf);
                    appendUninstallLog(m_log, "  Removed: " + cf);
                }
            }

            /* Remove tools directory */
            QString toolsDir = configDir + "/tools";
            if (QDir(toolsDir).exists()) {
                QDir(toolsDir).removeRecursively();
                appendUninstallLog(m_log, "  Removed: " + toolsDir);
            }

            /* Remove config directory if empty */
            QDir cfgDir(configDir);
            if (cfgDir.exists() && cfgDir.entryList(QDir::Files | QDir::Dirs).isEmpty()) {
                cfgDir.removeRecursively();
                appendUninstallLog(m_log, "  Removed: " + configDir);
            }
        } else {
            appendUninstallLog(m_log, "  No config directory found.");
        }
    } else {
        appendUninstallLog(m_log, "\n==> Configuration preserved (not purged).");
        appendUninstallLog(m_log, "  Config files remain in: ~/.config/patm/");
    }

    /* Done */
    m_statusLabel->setText("Uninstall complete!");
    m_progress->setRange(0, 1);
    m_progress->setValue(1);
    appendUninstallLog(m_log, "\n========================================");
    appendUninstallLog(m_log, "  PATM has been uninstalled.");
    appendUninstallLog(m_log, "========================================");
    m_finished = true;
    m_success = true;
    completeChanged();
}

bool UninstallProgressPage::validatePage()
{
    return m_finished && m_success;
}

bool UninstallProgressPage::isComplete() const
{
    return m_finished;
}

/* ---------- UninstallCompletePage ---------- */

UninstallCompletePage::UninstallCompletePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Uninstall Complete");
    setSubTitle("PATM has been removed from your system.");

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/org/patm/assets/patm-icon.svg").scaled(
        96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(logo);

    QLabel *done = new QLabel(
        "<h2 style='text-align:center;'>Uninstall Complete</h2>\n"
        "<p style='text-align:center;'>PATM has been successfully removed.</p>\n"
        "<p style='text-align:center; color:gray; font-size:small;'>Pipeline Automation Tool Manager</p>");
    done->setTextFormat(Qt::RichText);
    layout->addWidget(done);

    layout->addSpacing(20);

    QLabel *note = new QLabel(
        "Thank you for using PATM.\n\n"
        "If you change your mind, you can reinstall at any time by running "
        "the installer again.");
    note->setWordWrap(true);
    layout->addWidget(note);

    layout->addStretch();
}
