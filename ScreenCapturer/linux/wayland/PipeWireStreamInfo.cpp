/*---------------------------------------------------------*\
| PipeWireStreamInfo.cpp                                    |
|                                                           |
|   OpenRGB Effects Plugin PipeWire Stream Info             |
|                                                           |
|   This file is part of the OpenRGB Effects Plugin project |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "PipeWireStreamInfo.h"

QDBusArgument &operator<<(QDBusArgument &arg, const PipeWireStreamInfoList& /*list*/){return arg;}

const QDBusArgument &operator>>(const QDBusArgument &arg, PipeWireStreamInfoList& list)
{
    if(arg.currentType() != QDBusArgument::ArrayType) {
        return arg;
    }

    arg.beginArray();

    while(!arg.atEnd()){

        struct PipeWireStreamInfo info;

        arg.beginStructure();
        while(!arg.atEnd()){

            switch(arg.currentType()){

            case QDBusArgument::BasicType:
                info.node_id = arg.asVariant().toUInt();
                break;

            case QDBusArgument::MapType:
                arg.beginMap();

                while(!arg.atEnd()){
                    arg.beginMapEntry();

                    QString key;
                    QDBusVariant value;
                    arg >> key >> value;

                    if(key == "source_type") {
                        info.source_type = value.variant().toUInt();
                    }
                    else if (key == "position"){
                        QPair<quint32,quint32> pair= qdbus_cast<QPair<quint32,quint32>>(value.variant());
                        info.x = pair.first;
                        info.y = pair.second;
                    }
                    else if (key == "size"){
                        QPair<quint32,quint32> pair= qdbus_cast<QPair<quint32,quint32>>(value.variant());
                        info.width = pair.first;
                        info.height = pair.second;
                    }

                    arg.endMapEntry();
                }

                arg.endMap();
                break;
            default:
                break;
            }
        }
        arg.endStructure();

        list.push_back(info);
    }
    arg.endArray();

    return arg;
}
