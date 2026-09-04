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

#ifndef PATM_INSTALLER_WIZARD_H
#define PATM_INSTALLER_WIZARD_H

#include <QWizard>

class QStackedWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QProgressBar;
class QPlainTextEdit;

struct SqlEngine {
    QString name;
    QString displayName;
    QString packageName;
    QString binary;
    bool installed;
    bool selected;
};

class InstallerWizard : public QWizard
{
    Q_OBJECT
public:
    explicit InstallerWizard(QWidget *parent = nullptr);

    QString installPath() const;
    QList<SqlEngine> engines() const;
    bool createDesktopShortcut() const;
    bool autoStartDaemon() const;
    bool autoOpenApp() const;
    QByteArray sudoPassword() const;

    static bool isInstalled(const QString &binary);
    static QString detectPkgManager();
    static int runPrivileged(const QByteArray &password,
                             const QString &program,
                             const QStringList &args,
                             QByteArray *stdOut = nullptr,
                             QByteArray *stdErr = nullptr);

private slots:
    void onCurrentPageChanged(int id);

private:
    void setupPages();

    int m_pageWelcome;
    int m_pageLicense;
    int m_pageLocation;
    int m_pageEngines;
    int m_pageProgress;
    int m_pageComplete;
    QByteArray m_sudoPassword;

    friend class ProgressPage;
};

class WelcomePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit WelcomePage(QWidget *parent = nullptr);
};

class LicensePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit LicensePage(QWidget *parent = nullptr);
    bool validatePage() override;
};

class LocationPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit LocationPage(QWidget *parent = nullptr);
    bool validatePage() override;
    QString installPath() const;

private slots:
    void onBrowse();

private:
    QLineEdit *m_pathEdit;
};

class EnginesPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit EnginesPage(QList<SqlEngine> *engines, QWidget *parent = nullptr);
    bool validatePage() override;
    QList<SqlEngine> engines() const { return *m_engines; }

private:
    QList<SqlEngine> *m_engines;
    QList<QCheckBox *> m_checks;
};

class ProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ProgressPage(QWidget *parent = nullptr);
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

private slots:
    void runInstall();
    void onProcessOutput();
    void onProcessFinished(int exitCode);

private:
    QProgressBar *m_progress;
    QPlainTextEdit *m_log;
    QLabel *m_statusLabel;
    bool m_finished;
    bool m_success;
};

class CompletePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit CompletePage(QWidget *parent = nullptr);
    bool createShortcut() const;
    bool autoDaemon() const;
    bool autoOpen() const;

private:
    QCheckBox *m_shortcutCheck;
    QCheckBox *m_daemonCheck;
    QCheckBox *m_autoOpenCheck;
};

/* ---------- Uninstaller ---------- */

class UninstallerWizard : public QWizard
{
    Q_OBJECT
public:
    explicit UninstallerWizard(QWidget *parent = nullptr);

    bool purgeConfigs() const;
    QString detectedInstallPath() const;
    QByteArray sudoPassword() const;

private slots:
    void onCurrentPageChanged(int id);

private:
    void setupPages();

    int m_pageConfirm;
    int m_pagePurge;
    int m_pageProgress;
    int m_pageComplete;
    QString m_installPath;
    QByteArray m_sudoPassword;

    friend class UninstallProgressPage;
};

class UninstallConfirmPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit UninstallConfirmPage(const QString &installPath, QWidget *parent = nullptr);
    bool validatePage() override;

private:
    QLabel *m_pathLabel;
};

class UninstallPurgePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit UninstallPurgePage(QWidget *parent = nullptr);
    bool purgeConfigs() const;

private:
    QCheckBox *m_purgeCheck;
};

class UninstallProgressPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit UninstallProgressPage(QWidget *parent = nullptr);
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

private slots:
    void runUninstall();

private:
    QProgressBar *m_progress;
    QPlainTextEdit *m_log;
    QLabel *m_statusLabel;
    bool m_finished;
    bool m_success;
};

class UninstallCompletePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit UninstallCompletePage(QWidget *parent = nullptr);
};

#endif
