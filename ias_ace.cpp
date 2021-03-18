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

// 02 04 31 31 31 31 00
// size = 7
void DeRestPluginPrivate::handleIasAceClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }
    
    DBG_Printf(DBG_INFO, "Debug Keypad : Address 0x%016llX Payload %s, command 0x%02X\n", ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()), zclFrame.commandId());

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if ((zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        return;
    }

    if (zclFrame.commandId() == CMD_ARM)
    {
        
        DBG_Printf(DBG_INFO, "Debug Keypad : Arm command, size %d\n",zclFrame.payload().size());
        
        quint8 armMode;
        quint16 length = zclFrame.payload().size() - 2;
        QString code;
        quint8 zoneId;
        quint8 codeTemp;
        
        quint8 dummy;

        //  Arm mode
        //-------------    
        // 0x00 Disarm    
        // 0x01 Arm Day/Home Zones Only
        // 0x02 Arm Night/Sleep Zones Only
        // 0x03 Arm All Zones

        //Arm Mode
        stream >> armMode;
        
        if (length > 1)
        {
            // This part can vary, according to device
            
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
        
        DBG_Printf(DBG_INFO, "Debug Keypad : Arm command, Arm mode %d, code %s, Zone id%d\n", armMode , qPrintable(code) ,zoneId);

        if (armMode <= 3)
        {
            sendArmResponse(ind, zclFrame, armMode);
        }

        return;
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
        DBG_Printf(DBG_INFO, "Debug Keypad : Sending answer\n");
        sendGetPanelStatusResponse(ind, zclFrame);
    }
    else if (zclFrame.commandId() == CMD_GET_BYPASSED_ZONE_LIST)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_STATUS)
    {
    }

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
}

void DeRestPluginPrivate::sendArmResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 armMode)
{
    quint8 armNotification = 0xFF;

    if (armMode <= 3)
    {
        // 0: All zones disarmed, 1: Only day/home zones armed, 2: Only night/sleep zones armed, 3: All zones armed
        armNotification = armMode;
    }
    else if (armMode > 3 && armMode <= 6)
    {
        // 4 - 6 not supported
        return;
    }
    else
    {
        // invalid
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

        stream << armNotification;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO_L2, "[IAS ACE] - Failed to send IAS ACE arm reponse.\n");
    }
}

void DeRestPluginPrivate::sendGetPanelStatusResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
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

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

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

        stream << (quint8) 0x00; // Panel status
        stream << (quint8) 0x00; // Seconds Remaining
        stream << (quint8) 0x00; // Audible Notification
        stream << (quint8) 0x00; // Alarm status

    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO_L2, "[IAS ACE] - Failed to send IAS ACE get panel reponse.\n");
    }
}
