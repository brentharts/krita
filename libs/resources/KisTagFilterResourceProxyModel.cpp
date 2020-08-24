/*
 * Copyright (C) 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "KisTagFilterResourceProxyModel.h"

#include <QDebug>

#include <KisResourceModelProvider.h>
#include <KisResourceModel.h>
#include <KisTagResourceModel.h>
#include <KisTagModel.h>

#include <kis_debug.h>
#include <KisResourceSearchBoxFilter.h>

struct KisTagFilterResourceProxyModel::Private
{
    Private()
        : filter(new KisResourceSearchBoxFilter())
    {
    }

    QString resourceType;

    KisTagModel *tagModel {0}; // Not sure whether we'll need this
    KisResourceModel *resourceModel {0}; // This is the source model if we are _not_ filtering by tag
    KisTagResourceModel *tagResourceModel {0}; // This is the source model if we are filtering by tag

    QScopedPointer<KisResourceSearchBoxFilter> filter;
    bool filterInCurrentTag {false};

};

KisTagFilterResourceProxyModel::KisTagFilterResourceProxyModel(const QString &resourceType, QObject *parent)
    : QSortFilterProxyModel(parent)
    , d(new Private)
{
    d->resourceType = resourceType;
    d->tagModel = KisResourceModelProvider::tagModel(resourceType);
    d->resourceModel = KisResourceModelProvider::resourceModel(resourceType);
    d->tagResourceModel = KisResourceModelProvider::tagResourceModel(resourceType);

    setSourceModel(d->resourceModel);
}

KisTagFilterResourceProxyModel::~KisTagFilterResourceProxyModel()
{
    delete d;
}

void KisTagFilterResourceProxyModel::setResourceModel(KisResourceModel *resourceModel)
{
    d->resourceModel = resourceModel;
}

KoResourceSP KisTagFilterResourceProxyModel::resourceForIndex(QModelIndex index) const
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->resourceForIndex(mapToSource(index));
    }
    return 0;
}

QModelIndex KisTagFilterResourceProxyModel::indexForResource(KoResourceSP resource) const
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return mapFromSource(source->indexForResource(resource));
    }
    return QModelIndex();
}

bool KisTagFilterResourceProxyModel::setResourceInactive(const QModelIndex &index)
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->setResourceInactive(mapToSource(index));
    }
    return false;
}

bool KisTagFilterResourceProxyModel::importResourceFile(const QString &filename)
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->importResourceFile(filename);
    }
    return false;
}

bool KisTagFilterResourceProxyModel::addResource(KoResourceSP resource, const QString &storageId)
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->addResource(resource, storageId);
    }
    return false;
}

bool KisTagFilterResourceProxyModel::updateResource(KoResourceSP resource)
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->updateResource(resource);
    }
    return false;
}

bool KisTagFilterResourceProxyModel::renameResource(KoResourceSP resource, const QString &name)
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->renameResource(resource, name);
    }
    return false;
}

bool KisTagFilterResourceProxyModel::setResourceMetaData(KoResourceSP resource, QMap<QString, QVariant> metadata)
{
    KisAbstractResourceModel *source = dynamic_cast<KisAbstractResourceModel*>(sourceModel());
    if (source) {
        return source->setResourceMetaData(resource, metadata);
    }
    return false;
}

void KisTagFilterResourceProxyModel::setTag(const KisTagSP tag)
{
    if (!tag.isNull()) {
        if (tag->url() != "All Untagged") {
            d->tagResourceModel->setTagsFilter(QVector<KisTagSP>() << tag);
            setSourceModel(d->tagResourceModel);
        }
        else {
            setSourceModel(d->resourceModel);
            d->resourceModel->showOnlyUntaggedResources(true);
        }
    }
    else {
        setSourceModel(d->resourceModel);
        d->resourceModel->showOnlyUntaggedResources(false);
    }
    invalidateFilter();
}

void KisTagFilterResourceProxyModel::setSearchText(const QString& searchText)
{
    d->filter->setFilter(searchText);
    invalidateFilter();
}

void KisTagFilterResourceProxyModel::setFilterInCurrentTag(const bool filterInCurrentTag)
{
    d->filterInCurrentTag = filterInCurrentTag;
    invalidateFilter();
}

bool KisTagFilterResourceProxyModel::filterAcceptsColumn(int /*source_column*/, const QModelIndex &/*source_parent*/) const
{
    return true;
}

bool KisTagFilterResourceProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    // If there's a tag set to filter on, we use the tagResourceModel, so that already filters for the tag
    // Here, we only have to filter by the search string.

    QModelIndex idx = sourceModel()->index(source_row, KisAbstractResourceModel::Name, source_parent);

    QString resourceName = sourceModel()->data(idx, Qt::UserRole + KisAbstractResourceModel::Name).toString();

    return d->filter->matchesResource(resourceName);

}

bool KisTagFilterResourceProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    QString nameLeft = sourceModel()->data(source_left, Qt::UserRole + KisAbstractResourceModel::Name).toString();
    QString nameRight = sourceModel()->data(source_right, Qt::UserRole + KisAbstractResourceModel::Name).toString();
    return nameLeft < nameRight;
}

