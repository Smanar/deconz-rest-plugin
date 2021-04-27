/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "device.h"
#include "device_compat.h"
#include "device_descriptions.h"
#include "device_ddf_init.h"
#include "database.h"
#include "utils/utils.h"

static QString uniqueIdFromTemplate(const QStringList &templ, const quint64 extAddress)
{
    bool ok = false;
    quint8 endpoint = 0;
    quint16 clusterId = 0;

    // <mac>-<endpoint>
    // <mac>-<endpoint>-<cluster>
    if (templ.size() > 1 && templ.first() == QLatin1String("$address.ext"))
    {
        endpoint = templ.at(1).toUInt(&ok, 0);

        if (ok && templ.size() > 2)
        {
            clusterId = templ.at(2).toUInt(&ok, 0);
        }
    }

    if (ok)
    {
        return generateUniqueId(extAddress, endpoint, clusterId);
    }

    return {};
}

/*! Creates a ResourceItem if not exist, initialized with \p ddfItem content.
 */
static ResourceItem *DEV_InitDeviceDescriptionItem(const DeviceDescription::Item &ddfItem, const std::vector<DB_ResourceItem> &dbItems, Resource *rsub)
{
    Q_ASSERT(rsub);
    Q_ASSERT(ddfItem.isValid());

    auto *item = rsub->item(ddfItem.descriptor.suffix);
    const auto uniqueId = rsub->item(RAttrUniqueId)->toString();

    if (item)
    {
        DBG_Printf(DBG_INFO, "sub-device: %s, has item: %s\n", qPrintable(uniqueId), ddfItem.descriptor.suffix);
    }
    else
    {
        DBG_Printf(DBG_INFO, "sub-device: %s, create item: %s\n", qPrintable(uniqueId), ddfItem.descriptor.suffix);
        item = rsub->addItem(ddfItem.descriptor.type, ddfItem.descriptor.suffix);

        DBG_Assert(item);
        if (!item)
        {
            return nullptr;
        }
    }

    Q_ASSERT(item);

    const auto dbItem = std::find_if(dbItems.cbegin(), dbItems.cend(), [&ddfItem](const auto &dbItem)
    {
        return ddfItem.name == dbItem.name;
    });

    if (item->lastSet().isValid() && ddfItem.isStatic)
    { }
    else if (dbItem != dbItems.cend())
    {
        item->setValue(dbItem->value);
        item->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem->timestampMs));
    }
    else if (ddfItem.defaultValue.isValid())
    {
        item->setValue(ddfItem.defaultValue);
    }

    item->setDdfItemHandle(ddfItem.handle);

    // check updates
    item->setIsPublic(ddfItem.isPublic);
    item->setAwake(ddfItem.awake);

    if (ddfItem.refreshInterval != DeviceDescription::Item::NoRefreshInterval)
    {
        item->setRefreshInterval(ddfItem.refreshInterval);
    }

    item->setParseFunction(nullptr);

    return item;
}

/*! Creates and initialises sub-device Resources and ResourceItems if not already present.

    This function can replace database and joining device initialisation.
 */
bool DEV_InitDeviceFromDescription(Device *device, const DeviceDescription &description)
{
    Q_ASSERT(device);
    Q_ASSERT(description.isValid());

    size_t subCount = 0;

    for (const auto &sub : description.subDevices)
    {
        Q_ASSERT(sub.isValid());

        const auto uniqueId = uniqueIdFromTemplate(sub.uniqueId, device->item(RAttrExtAddress)->toNumber());
        Resource *rsub = DEV_GetSubDevice(device, nullptr, uniqueId);

        if (!rsub)
        {
            rsub = DEV_InitCompatNodeFromDescription(device, sub, uniqueId);
        }

        if (!rsub)
        {
            DBG_Printf(DBG_INFO, "sub-device: %s, failed to setup: %s\n", qPrintable(uniqueId), qPrintable(sub.type));
            return false;
        }

        subCount++;

        auto *mf = rsub->item(RAttrManufacturerName);
        if (mf && mf->toString().isEmpty())
        {
            mf->setValue(DeviceDescriptions::instance()->constantToString(description.manufacturer));
        }

        // TODO storing should be done else where, since this is init code
        DB_StoreSubDevice(device->item(RAttrUniqueId)->toString(), uniqueId);
        DB_StoreSubDeviceItem(rsub, rsub->item(RAttrManufacturerName));
        DB_StoreSubDeviceItem(rsub, rsub->item(RAttrModelId));

        const auto dbItems = DB_LoadSubDeviceItems(uniqueId);

        for (const auto &ddfItem : sub.items)
        {
            auto *item = DEV_InitDeviceDescriptionItem(ddfItem, dbItems, rsub);
            if (!item)
            {
                continue;
            }

//            if (item->descriptor().suffix == RConfigCheckin)
//            {
//                StateChange stateChange(StateChange::StateWaitSync, SC_WriteZclAttribute, sub.uniqueId.at(1).toUInt());
//                stateChange.addTargetValue(RConfigCheckin, ddfItem.defaultValue);
//                stateChange.setChangeTimeoutMs(1000 * 60 * 60);
//                rsub->addStateChange(stateChange);
//            }
        }
    }

    if (description.sleeper >= 0)
    {
        device->item(RAttrSleeper)->setValue(description.sleeper == 1);
    }

    return subCount == description.subDevices.size();
}


bool DEV_InitDeviceBasic(Device *device)
{
    const auto dbItems = DB_LoadSubDeviceItemsOfDevice(device->item(RAttrUniqueId)->toString());

    size_t found = 0;
    std::array<const char*, 2> poi = { RAttrManufacturerName, RAttrModelId };
    for (const auto &dbItem : dbItems)
    {
        for (const char *suffix: poi)
        {
            if (dbItem.name != suffix)
            {
                continue;
            }

            auto *item = device->item(suffix);

            if (item)
            {
                item->setValue(dbItem.value);
                item->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs));
                found++;
            }
        }
    }

    return found == poi.size();
}
