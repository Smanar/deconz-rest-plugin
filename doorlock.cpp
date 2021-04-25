/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QJsonObject>
#include <QJsonDocument>
 
#include "de_web_plugin_private.h"

#define OPERATION_EVENT_NOTIFICATON quint8(0x20)

#define COMMAND_READ_PIN quint8(0x06)
#define COMMAND_SET_PIN quint8(0x05)
#define COMMAND_CLEAR_PIN quint8(0x07)

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

    //Defaut response
    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    
    bool stateUpdated;

    if (zclFrame.isClusterCommand() &&
       (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        if (zclFrame.commandId() == COMMAND_READ_PIN)
        {
            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian);
            
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
            
            DBG_Printf(DBG_IAS, "[Door lock] - Read PIN command received, User ID: %d, code: %s, Status: %d, Type %d\n", userID , qPrintable(code) ,status, type);

            QString data = QString("\"%1\":{\"id\":%1,\"status\":%2,\"type\":%3,\"code\":%4}").arg(userID).arg(status).arg(type).arg(code);
            
            
            if (false)
            {
                //Transform qsting to json
                QString data = QLatin1String("{}");
                
                QJsonObject jsonObj;
                QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());

                // check validity of the document
                if(!doc.isNull())
                {
                    if(doc.isObject())
                    {
                        jsonObj = doc.object();        
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "Door lock debug : Not an object\n");
                    }
                }
                else
                {
                    DBG_Printf(DBG_INFO, "Door lock debug : Json error\n");
                }
                
                // Make magic
                QVariantList user_list = jsonObj.toVariantList();
                
                
                //QVariantMap json_map = jsonObj.toVariantMap();
                //QVariantMap user = root_map["stats"].toMap();
                //foreach(QVariantMap user, json_map)
                //{
                //    QVariantMap user = city.toMap();
                //}
                
     
                //Transform Json to qstring
                QJsonDocument doc(jsonObj);
                QString data = strJson(doc.toJson(QJsonDocument::Compact));
            }

            
            ResourceItem *item = sensorNode->item(RStatePin);

            if (item )
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
    
}
