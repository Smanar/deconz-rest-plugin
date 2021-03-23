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

void DeRestPluginPrivate::handleDoorLockClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    
    QString zclPayload = zclFrame.payload().isEmpty() ? "None" : qPrintable(zclFrame.payload().toHex().toUpper());
    DBG_Printf(DBG_INFO, "Door lock debug 0x%016llX, command  0x%02X, payload \n", ind.srcAddress().ext(), zclFrame.commandId(), qPrintable(zclPayload) );

    if (zclFrame.commandId() == OPERATION_EVENT_NOTIFICATON &&
        zclFrame.isClusterCommand() &&
        (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) == 0)
    {
        {
            //auto *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
            
            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian

            quint8 source;
            quint8 code;
            quint16 pin;
            quint8 localtime;
            
            stream >> source;
            stream >> code;
            stream >> pin;
            stream >> localtime;
            
            DBG_Printf(DBG_INFO, "Door lock notifications > source: 0x%02X, code: 0x%02X, pin: 0x%04X local time:0x%02X", source, code, pin, localtime);

        }
    }
}
