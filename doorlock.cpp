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
#include "json.h"
#include "doorlock.h"

const QStringList EventSourceList({"keypad","rf","manual","rfid"});
const QStringList EventCodeList({
    "Unknown","Lock","Unlock","LockFailureInvalidPINorID","LockFailureInvalidSchedule","UnlockFailureInvalidPINorID","UnlockFailureInvalidSchedule","OneTouchLock","KeyLock",
    "KeyUnlock","AutoLock","ScheduleLock","ScheduleUnlock","Manual Lock","Manual Unlock","Non-Access User Operational Event"
    });

// User Status
//------------
// 0x01 Occupied / Enabled (Access Given)
// 0x03 Occupied / Disabled
// 0xFF Not Supported

// User Type
//----------
// 0x00 Unrestricted User (default)
// 0x01 Year Day Schedule User
// 0x02 Week Day Schedule User
// 0x03 Master User
// 0x04 Non Access User
// 0xFF Not Supported

void DeRestPluginPrivate::handleDoorLockClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    
    QString zclPayload = zclFrame.payload().isEmpty() ? "None" : qPrintable(zclFrame.payload().toHex().toUpper());
    DBG_Printf(DBG_INFO, "Door lock debug 0x%016llX, command  0x%02X, payload %s\n", ind.srcAddress().ext(), zclFrame.commandId(), qPrintable(zclPayload) );
    
    
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHADoorLock"));
    if (!sensorNode)
    {
        return;
    }

    //Defaut response
    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    
    bool stateUpdated;

    if (zclFrame.isClusterCommand())
    {
        if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)
        {
            
            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian);
            
            if (zclFrame.commandId() == COMMAND_SET_PIN)
            {
                quint8 status;
                stream >> status;
                
                // 0x00 = Success
                // 0x01 = General failure
                // 0x02 = Memory full
                // 0x03 = Duplicate Code error
                
                DBG_Printf(DBG_INFO, "[Door lock] - Set PIN command received, Status: %d\n", status);
                
            }
            else if (zclFrame.commandId() == COMMAND_CLEAR_PIN)
            {
                quint8 status;
                stream >> status;
                
                DBG_Printf(DBG_INFO, "[Door lock] - Clear PIN command received, Status: %d\n", status);
                
            }
            else if (zclFrame.commandId() == COMMAND_READ_PIN)
            {
                // sample payload : 0300 01 00 04 31323334
                
                quint16 length = zclFrame.payload().size() - 4;
                
                quint16 userID;
                quint8 status;
                quint8 type;
                QString code;
                quint8 codeTemp;
                
                stream >> userID;
                stream >> status;
                stream >> type;

                if (length > 1)
                {
                    
                    // skip Code lenght, use payload lenght instead
                    stream >> codeTemp;
                    length -= 1;
                    
                    for (; length > 0; length--)
                    {
                        stream >> codeTemp;
                        code.append(QChar(codeTemp));
                    }
                }
                
                DBG_Printf(DBG_INFO, "[Door lock] - Read PIN command received, User ID: %d, code: %s, Status: %d, Type %d\n", userID , qPrintable(code) ,status, type);

                QString data;
                
                ResourceItem *item = sensorNode->item(RStatePin);

                if (item && !item->toString().isEmpty())
                {
                    data = item->toString();
                }
                else
                {
                    data = QLatin1String("[]");
                }
                
                data = data.replace(QLatin1String("\\\""), QLatin1String("\""));
                
                DBG_Printf(DBG_INFO, "Door lock debug : data %s\n", qPrintable(data));

                if (true)
                {
                    //Transform qstring to json
                    QVariant var = Json::parse(data);
                    QVariantList list = var.toList();
                    QVariantList list2;
                    
                    bool exist = false;
                    quint16 id;
                    
                    foreach (const QVariant & v, list)
                    {
                        QVariantMap map = v.toMap();
                        
                        if (map["id"].type() == QVariant::Double)
                        {
                            id = map["id"].toInt();
                            
                            //If exist, update
                            if (id == userID)
                            {
                                map["status"] = status;
                                map["type"] = type;
                                map["code"] = code;
                                
                                exist = true;
                            }
                        }
                        list2.append(map);
                    }
                    
                    //If not exist, add it
                    if (!exist)
                    {
                        QVariantMap map2;
                        map2.insert("id", userID);
                        map2.insert("status",status);
                        map2.insert("type", type);
                        map2.insert("code", code);
                        list2.append(map2);
                    }
         
                    //Transform Json array to qstring
                    data = Json::serialize(list2);
                }

                if (item)
                {
                    item->setValue(data);
                    Event e(RSensors, RStatePin, sensorNode->id(), item);
                    enqueueEvent(e);
                    stateUpdated = true;
                }
                
            }
            else if (zclFrame.commandId() == OPERATION_EVENT_NOTIFICATON)
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

                if (item)
                {
                    char s[5];
                    sprintf(s, "%04d", pin);
                    QString action = QString("source:%1,code:%2,pin:%3").arg(sourcename).arg(codename).arg(s);
                    item->setValue(action);
                    Event e(RSensors, RStateNotification, sensorNode->id(), item);
                    enqueueEvent(e);
                    stateUpdated = true;
                }

            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "Door lock debug 9\n");
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
    
}


/*! Add doorlock Get Pin task to the queue.

    \param task - the task item
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::addTaskDoorLockPin(TaskItem &task, quint8 command, quint16 userID, QVariantMap map)
{
    task.taskType = TaskDoorUnlock;

    task.req.setClusterId(DOOR_LOCK_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(command); // Get Pin
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        
        if (command == COMMAND_SET_PIN)
        {
            bool ok2;
            quint8 status;
            quint8 type;

            status = static_cast<qint8>(map["status"].toUInt(&ok2));
            if (!ok2) { return false; }
            type = static_cast<qint8>(map["type"].toUInt(&ok2));
            if (!ok2) { return false; }
            
            stream << userID;
            stream << status;
            stream << type;

            const QByteArray id = map["code"].toString().toLatin1();
            const quint8 length = id.length();
            
            stream << length;
            for (uint i = 0; i < length; i++)
            {
                stream << (quint8) id[i];
            }
        }
        else
        {
            stream << userID;
        }
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
