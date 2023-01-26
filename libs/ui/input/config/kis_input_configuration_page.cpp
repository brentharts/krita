/*
 * This file is part of the KDE project
 * SPDX-FileCopyrightText: 2013 Arjen Hiemstra <ahiemstra@heimr.nl>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_input_configuration_page.h"
#include "ui_kis_input_configuration_page.h"

#include <QDebug>
#include <QDir>
#include <QMap>

#include "input/kis_input_profile_manager.h"
#include "input/kis_input_profile.h"
#include "input/kis_shortcut_configuration.h"
#include "input/kis_abstract_input_action.h"
#include "kis_edit_profiles_dialog.h"
#include "kis_input_profile_model.h"
#include "kis_input_configuration_page_item.h"
#include <KoResourcePaths.h>


#include "kis_icon_utils.h"

struct KisInputConfigurationPage::Private {
    QMap<KisAbstractInputAction *, KisInputConfigurationPageItem*> actionInputConfigurationMap;
};

KisInputConfigurationPage::KisInputConfigurationPage(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , m_d(new Private)
{
    ui = new Ui::KisInputConfigurationPage;
    ui->setupUi(this);

    ui->profileComboBox->setModel(new KisInputProfileModel(ui->profileComboBox));
    updateSelectedProfile();
    connect(ui->profileComboBox, SIGNAL(currentIndexChanged(QString)), SLOT(changeCurrentProfile(QString)));

    ui->editProfilesButton->setIcon(KisIconUtils::loadIcon("document-edit"));

    connect(ui->editProfilesButton, SIGNAL(clicked(bool)), SLOT(editProfilesButtonClicked()));
    connect(KisInputProfileManager::instance(), SIGNAL(profilesChanged()), SLOT(updateSelectedProfile()));
    connect(KisInputProfileManager::instance(), SIGNAL(currentProfileChanged()), SLOT(checkForConflicts()));

    QList<KisAbstractInputAction *> actions = KisInputProfileManager::instance()->actions();
    Q_FOREACH(KisAbstractInputAction * action, actions) {
        KisInputConfigurationPageItem *item = new KisInputConfigurationPageItem(this);
        item->setAction(action);
        ui->configurationItemsArea->addWidget(item);
        m_d->actionInputConfigurationMap.insert(action, item);
        connect(item,
                &KisInputConfigurationPageItem::inputConfigurationChanged,
                this,
                &KisInputConfigurationPage::checkForConflicts,
                Qt::UniqueConnection);
    }
    checkForConflicts();
    ui->configurationItemsArea->addStretch(20); // ensures listed input are on top

    QScroller *scroller = KisKineticScroller::createPreconfiguredScroller(ui->scrollArea);
    if (scroller) {
        connect(scroller, SIGNAL(stateChanged(QScroller::State)),
                this, SLOT(slotScrollerStateChanged(QScroller::State)));
    }
}

KisInputConfigurationPage::~KisInputConfigurationPage() = default;

void KisInputConfigurationPage::saveChanges()
{
    KisInputProfileManager::instance()->saveProfiles();
}

void KisInputConfigurationPage::revertChanges()
{
    KisInputProfileManager::instance()->loadProfiles();
}

void KisInputConfigurationPage::checkForConflicts()
{
    // reset all of them first.
    Q_FOREACH (KisInputConfigurationPageItem *pageItem, m_d->actionInputConfigurationMap.values()) {
        pageItem->setWarningEnabled(false);
    }
    const QList<KisShortcutConfiguration *> conflictingShortcuts =
        KisInputProfileManager::instance()->getConflictingShortcuts(
            KisInputProfileManager::instance()->currentProfile());

    if (conflictingShortcuts.isEmpty()) {
        return;
    }
    Q_FOREACH (KisShortcutConfiguration *shortcut, conflictingShortcuts) {
        if (shortcut->action()) {
            if (m_d->actionInputConfigurationMap.contains(shortcut->action())) {
                m_d->actionInputConfigurationMap[shortcut->action()]->setWarningEnabled(true);
            } else {
                qWarning() << "KisInputConfigurationPageItem does not exist for the specified action:"
                           << shortcut->action()->name();
            }
        } else {
            qWarning() << "Action not set for the given shortcut.";
        }
    }
}

void KisInputConfigurationPage::setDefaults()
{
    QDir profileDir(KoResourcePaths::saveLocation("data", "input/", false));

    if (profileDir.exists()) {
        QStringList entries = profileDir.entryList(QStringList() << "*.profile", QDir::Files | QDir::NoDotAndDotDot);
        Q_FOREACH(const QString & file, entries) {
            profileDir.remove(file);
        }

        KisInputProfileManager::instance()->loadProfiles();
    }
}

void KisInputConfigurationPage::editProfilesButtonClicked()
{
    KisEditProfilesDialog dialog;
    dialog.exec();
}

void KisInputConfigurationPage::updateSelectedProfile()
{
    if (KisInputProfileManager::instance()->currentProfile()) {
        ui->profileComboBox->setCurrentItem(KisInputProfileManager::instance()->currentProfile()->name());
    }
}

void KisInputConfigurationPage::changeCurrentProfile(const QString &newProfile)
{
    KisInputProfileManager::instance()->setCurrentProfile(KisInputProfileManager::instance()->profile(newProfile));
}
