/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

#define OPERATION_EVENT_NOTIFICATON quint8(0x20)

#define COMMAND_READ_PIN quint8(0x06)

const QStringList EventSourceList({"keypad","rf","manual","rfid"});
const QStringList EventCodeList({
    "Unknown","Lock","Unlock","LockFailureInvalidPINorID","LockFailureInvalidSchedule","UnlockFailureInvalidPINorID","UnlockFailureInvalidSchedule","OneTouchLock","KeyLock",
    "KeyUnlock","AutoLock","ScheduleLock","ScheduleUnlock","Manual Lock","Manual Unlock","Non-Access User Operational Event"
    });

void DeRestPluginPrivate::handleDoorLockClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    
    QString zclPayload = zclFrame.payload().isEmpty() ? "None" : qPrintable(zclFrame.payload().toHex().toUpper());
    DBG_Printf(DBG_INFO, "Door lock debug 0x%016llX, command  0x%02X, payload %s\n", ind.srcAddress().ext(), zclFrame.commandId(), qPrintable(zclPayload) );
    
    
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHADoorLock"));
    if (!sensorNode)
    {
        return;
    }
    
    bool stateUpdated;
    
    if (zclFrame.commandId() == COMMAND_READ_PIN &&
        zclFrame.isClusterCommand() &&
        (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        DBG_Printf(DBG_INFO, "Door lock debug 1\n" );
    }
    
    if (zclFrame.commandId() == COMMAND_READ_PIN &&
        zclFrame.isClusterCommand() &&
        !(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        DBG_Printf(DBG_INFO, "Door lock debug 2\n" );
    }

    if (zclFrame.commandId() == OPERATION_EVENT_NOTIFICATON &&
        zclFrame.isClusterCommand() &&
        (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        {
            
            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian);

            quint8 source;
            quint8 code;
            quint16 userID;
            quint8 pin;
            quint8 localtime;
            
            stream >> source;
            stream >> code;
            stream >> userID;
            stream >> pin;
            stream >> localtime;
            
            DBG_Printf(DBG_INFO, "Door lock notifications > source: 0x%02X, code: 0x%02X, pin: 0x%04X local time:0x%02X", source, code, pin, localtime);
            
            //Source name
            QString sourcename;
            if (source > EventSourceList.size()) {
                sourcename =  QString("unknow");
            }
            else
            {
                sourcename =  EventSourceList[source];
            }

            //code name
            QString codename;
            if (code > EventCodeList.size()) {
                codename =  QString("unknow");
            }
            else
            {
                codename =  EventCodeList[code];
            }
            
            ResourceItem *item = sensorNode->item(RStateNotification);

            if (item )
            {
                QString action = QString("source:%1,code:%2,pin:%3").arg(sourcename).arg(codename).arg(pin);
                item->setValue(action);
                Event e(RSensors, RStateNotification, sensorNode->id(), item);
                enqueueEvent(e);
                stateUpdated = true;
            }

        }
    }
    
    if (stateUpdated)
    {
        sensorNode->updateStateTimestamp();
        enqueueEvent(Event(RSensors, RStateLastUpdated, sensorNode->id()));
        updateSensorEtag(&*sensorNode);
        sensorNode->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
    }

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    
}
