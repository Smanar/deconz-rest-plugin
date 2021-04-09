#include <QString>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

// server send
#define CMD_ARM_RESPONSE 0x00
#define CMD_GET_ZONE_ID_MAP_RESPONSE 0x01
#define CMD_GET_ZONE_INFORMATION_RESPONSE 0x02
#define CMD_ZONE_STATUS_CHANGED 0x03
#define CMD_PANEL_STATUS_CHANGED 0x04
#define CMD_GET_PANEL_STATUS_RESPONSE 0x05
#define CMD_SET_BYPASSED_ZONE_LIST 0x06
#define CMD_BYPASS_RESPONSE 0x07
#define CMD_GET_ZONE_STATUS_RESPONSE 0x08
// server receive
#define CMD_ARM 0x00
#define CMD_BYPASS 0x01
#define CMD_EMERGENCY 0x02
#define CMD_FIRE 0x03
#define CMD_PANIC 0x04
#define CMD_GET_ZONE_ID_MAP 0x05
#define CMD_GET_ZONE_INFORMATION 0x06
#define CMD_GET_PANEL_STATUS 0x07
#define CMD_GET_BYPASSED_ZONE_LIST 0x08
#define CMD_GET_ZONE_STATUS 0x09

//  Arm mode command
//-------------------
// 0x00 Disarm    
// 0x01 Arm Day/Home Zones Only
// 0x02 Arm Night/Sleep Zones Only
// 0x03 Arm All Zones

//  Arm mode response
//-------------------
// 0x00 All Zones Disarmed
// 0x01 Only Day/Home Zones Armed
// 0x02 Only Night/Sleep Zones Armed
// 0x03 All Zones Armed
// 0x04 Invalid Arm/Disarm Code
// 0x05 Not ready to arm
// 0x06 Already disarmed

//   Panel status
// --------------        
// 0x00 Panel disarmed (all zones disarmed) and ready to arm
// 0x01 Armed stay
// 0x02 Armed night
// 0x03 Armed away
// 0x04 Exit delay
// 0x05 Entry delay
// 0x06 Not ready to arm
// 0x07 In alarm
// 0x08 Arming Stay
// 0x09 Arming Night
// 0x0a Arming Away

// Alarm Status
// ------------
// 0x00 No alarm
// 0x01 Burglar
// 0x02 Fire
// 0x03 Emergency
// 0x04 Police Panic
// 0x05 Fire Panic
// 0x06 Emergency Panic (i.e., medical issue)

// Audible Notification
// ----------------------   
// 0x00 Mute (i.e., no audible notification)
// 0x01 Default sound
// 0x80-0xff Manufacturer specific


const QStringList PanelStatusList({
    "disarmed","armed_stay","armed_night","armed_away","exit_delay","entry_delay","not_ready_to_arm","in_alarm","arming_stay","arming_night","arming_away"
});
const QStringList ArmModeList({
    "disarmed","armed_stay","armed_night","armed_away"
});

void DeRestPluginPrivate::handleIasAceClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }
    
    DBG_Printf(DBG_IAS, "[IAS ACE] - Address 0x%016llX, Payload %s, Command 0x%02X\n", ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()), zclFrame.commandId());

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if ((zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        return;
    }
    
    //Defaut response
    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAAncillaryControl"));
    if (!sensorNode)
    {
        return;
    }
    
    ResourceItem *item;
    bool stateUpdated = false;

    if (zclFrame.commandId() == CMD_ARM)
    {
        quint8 armMode;
        quint16 length = zclFrame.payload().size() - 2;
        QString code;
        QString armcommand;
        quint8 zoneId;
        quint8 codeTemp;
        
        quint8 dummy;

        //Arm Mode
        stream >> armMode;
        
        if (armMode > ArmModeList.size()) {
            armcommand =  QString("unknow");
        }
        else
        {
            armcommand =  ArmModeList[armMode];
        }
        
        if (length > 1)
        {
            // This part can vary, according to device, for exemple keyfob have length = 0
            // the Arm/Disarm Code SHOULD be between four and eight alphanumeric characters in length.
            // The string encoding SHALL be UTF-8.
            
            // Code lenght
            stream >> dummy;
            length -= 1;
            
            //Arm/Disarm Code
            for (; length > 0; length--)
            {
                stream >> codeTemp;
                code.append(QChar(codeTemp));
            }
        }
        
        //Zone ID
        stream >> zoneId;
        
        DBG_Printf(DBG_IAS, "[IAS ACE] - Arm command received, arm mode: %d, code: %s, Zone id: %d\n", armMode , qPrintable(code) ,zoneId);
        
        if (!code.isEmpty())
        {
            item = sensorNode->item(RStateAction);

            if (item)
            {
                QString action = QString("%1,%2,%3").arg(armcommand).arg(code).arg(zclFrame.sequenceNumber());
                item->setValue(action);
                Event e(RSensors, RStateAction, sensorNode->id(), item);
                enqueueEvent(e);
                stateUpdated = true;
            }
        }
        
        //Jut memorise the value for the moment
        item = sensorNode->item(RStateArmMode);
        if (item)
        {
            item->setValue(armcommand);
            Event e(RSensors, RStateArmMode, sensorNode->id(), item);
            enqueueEvent(e);
            stateUpdated = true;
        }

        // Send the same value to confirm or error, no test yet
        if (armMode > 0x03) {
            armMode = 0x04; // Invalid
        }
        sendArmResponse(ind, zclFrame, armMode);

    }
    else if (zclFrame.commandId() == CMD_EMERGENCY)
    {
    }
    else if (zclFrame.commandId() == CMD_FIRE)
    {
    }
    else if (zclFrame.commandId() == CMD_PANIC)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_ID_MAP)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_INFORMATION)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_PANEL_STATUS)
    {
        quint8 PanelStatus;
        
        item = sensorNode->item(RStatePanel);
        if (item && !item->toString().isEmpty())
        {
            PanelStatus = PanelStatusList.indexOf(item->toString());
        }
        else
        {
            PanelStatus = 0x00;  // Disarmed
            DBG_Printf(DBG_IAS, "[IAS ACE] : error, can't get PanelStatus");
        }
        
        sendGetPanelStatusResponse(ind, zclFrame, PanelStatus);
        
        //Update too the presence detection
        if (sensorNode->modelId() == QLatin1String("URC4450BC0-X-R"))
        {
            Sensor *sensor2 = nullptr;
            sensor2 = getSensorNodeForAddressAndEndpoint(sensorNode->address(), sensorNode->fingerPrint().endpoint, QLatin1String("ZHAPresence"));
            if (sensor2)
            {
                item = sensor2->item(RStatePresence);
                item->setValue(true);
                sensor2->updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStatePresence, sensor2->id()));
                updateSensorEtag(&*sensor2);
            }
        }
        
    }
    else if (zclFrame.commandId() == CMD_GET_BYPASSED_ZONE_LIST)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_STATUS)
    {
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

void DeRestPluginPrivate::sendArmResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 armMode)
{
    //Not supported ?
    if ( armMode > 0x06)
    {
        return;
    }

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(endpoint());

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(CMD_ARM_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << armMode;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] - Failed to send IAS ACE arm reponse.\n");
    }
}

void DeRestPluginPrivate::sendGetPanelStatusResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame , quint8 PanelStatus)
{

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(endpoint());

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(CMD_GET_PANEL_STATUS_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                deCONZ::ZclFCDirectionServerToClient); // deCONZ::ZclFCDisableDefaultResponse

    // The Seconds Remaining parameter SHALL be provided if the Panel Status parameter has a value of 0x04
    // (Exit delay) or 0x05 (Entry delay).

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (quint8) PanelStatus; // Panel status
        
        if ((PanelStatus == 0x04) || ( PanelStatus == 0x05))
        {
            stream << (quint8) 0x05; // Seconds Remaining
        }
        else
        {
           stream << (quint8) 0x00; // Seconds Remaining 
        }
        stream << (quint8) 0x01; // Audible Notification
        stream << (quint8) 0x00; // Alarm status
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] - Failed to send IAS ACE get panel reponse.\n");
    }
}

bool DeRestPluginPrivate::addTaskPanelStatusChanged(TaskItem &task, const QString &mode)
{
    task.taskType = TaskIASACE;

    task.req.setClusterId(IAS_ACE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(CMD_PANEL_STATUS_CHANGED);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);
     // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    //data
    int PanelStatus = PanelStatusList.indexOf(mode);
    
    //Unknow mode ?
    if (PanelStatus < 0)
    {
        return false;
    }
    
    stream << static_cast<quint8>(PanelStatus);
    stream << (quint8) 0x00; // Seconds Remaining
    stream << (quint8) 0x01; // Audible Notification
    stream << (quint8) 0x00; // Alarm status

    // ZCL frame
    {
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
