/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "configuration.h"
#include "i18n.h"

#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_processRefreshService(new OS::ProcessRefreshService(this))
    , m_processesWidget(new ProcessesWidget(this->m_processRefreshService, this))
    , m_performanceWidget(new PerformanceWidget(this))
    , m_usersWidget(new UsersWidget(this->m_processRefreshService, this))
    , m_servicesWidget(new ServicesWidget(this))
{
    this->ui->setupUi(this);

    if (CFG->IsSuperuser)
    {
        // Make user aware of this
        this->setWindowTitle(this->windowTitle() + tr(" (superuser)"));
    }

    this->ui->processesLayout->addWidget(this->m_processesWidget);
    this->ui->performanceLayout->addWidget(this->m_performanceWidget);
    this->ui->usersLayout->addWidget(this->m_usersWidget);
    this->ui->servicesLayout->addWidget(this->m_servicesWidget);

    this->buildMenus();

    // Restore previous window layout
    if (!CFG->WindowGeometry.isEmpty())
        this->restoreGeometry(CFG->WindowGeometry);
    if (!CFG->WindowState.isEmpty())
        this->restoreState(CFG->WindowState);
    this->ui->tabWidget->setCurrentIndex(CFG->ActiveTab);
    this->updateTabActivity(this->ui->tabWidget->currentIndex());

    // Keep ActiveTab in sync as the user switches tabs
    connect(this->ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index)
    {
        CFG->ActiveTab = index;
        this->updateTabActivity(index);
    });

    connect(this->m_usersWidget, &UsersWidget::goToProcessRequested, this, [this](pid_t pid)
    {
        this->ui->tabWidget->setCurrentIndex(0);
        this->m_processesWidget->ClearSearchFilter();
        this->m_processesWidget->SelectProcessByPid(pid);
    });
}

MainWindow::~MainWindow()
{
    delete this->ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    CFG->WindowGeometry = this->saveGeometry();
    CFG->WindowState    = this->saveState();
    CFG->Save();
    QMainWindow::closeEvent(event);
}

void MainWindow::updateTabActivity(int index)
{
    // Tabs: 0=Processes, 1=Performance, 2=Users, 3=Services
    this->m_processesWidget->SetActive(index == 0);
    this->m_performanceWidget->SetActive(index == 1);
    this->m_usersWidget->SetActive(index == 2);
    this->m_servicesWidget->SetActive(index == 3);
}

void MainWindow::buildMenus()
{
    // Options → Language selector. Language changes are applied by restarting
    // the application so every widget, model and dialog is consistently rebuilt.
    QMenu *optionsMenu = this->ui->menubar->addMenu(tr("Options"));
    QMenu *languageMenu = optionsMenu->addMenu(tr("Language"));
    languageMenu->setToolTipsVisible(true);

    auto *group = new QActionGroup(languageMenu);
    group->setExclusive(true);

    const QString current = CFG->Language;
    const auto addLanguageAction = [this, languageMenu, group, current](const QString &code, const QString &label)
    {
        QAction *action = languageMenu->addAction(label);
        action->setCheckable(true);
        action->setData(code);
        action->setChecked(code == current);
        group->addAction(action);
    };

    addLanguageAction(QString(), tr("System default"));
    addLanguageAction(QStringLiteral("en"), tr("English"));
    addLanguageAction(QStringLiteral("zh_CN"), tr("Chinese (Simplified)"));

    connect(group, &QActionGroup::triggered, this, [this, group](QAction *action)
    {
        const QString newCode = action->data().toString();
        if (newCode == CFG->Language)
            return;

        const QString newName = action->text();
        const QString question = tr("Switch the interface language to \"%1\"?\n\nThe application needs to restart for the change to take effect.").arg(newName);
        const auto answer = QMessageBox::question(this, tr("Language"), question,
                                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
        {
            // Keep the previously active entry selected.
            for (QAction *other : group->actions())
                other->setChecked(other->data().toString() == CFG->Language);
            return;
        }

        CFG->Language = newCode;
        CFG->Save();
        I18n::restartApplication();
    });
}
