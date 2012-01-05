/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtAddOn.JsonDb 1.0
import "content"

Rectangle {
    width: 400
    height: 300
    property int fontsize: 20
    color: "black";

    JsonDb {
        id: jsondb
    }

    RssModel {
        id: rssModel
        onStatusChanged: {
            console.log("RssModel.onStatusChange status=" + status);
            if (rssModel.status == 1) {
                for (var i = 0; i < rssModel.count; i++) {
                    console.log("i=" + i + " " + JSON.stringify(rssModel.get(i)));
                    var obj = rssModel.get(i);
                    obj['_type'] = "FlickrImage";
                    jsondb.create(obj, function(info) { responses.append(info); console.log("Created object: " + JSON.stringify(info))},
                                  function (e) { console.error("Error creating image: " + e)});
                }
            }
        }
    }

    ListModel {
        id: responses
    }
    ListView {
        id: listView
        anchors.top: parent.top
        anchors.bottom: toolBar.top
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        width: parent.width
        model: responses
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 ;width: 200;}
        focus: true
        delegate: Row {
            spacing: 10
            Text {
                text: _uuid
                font.pointSize: fontsize
                color: "white"
            }
        }
    }

    ToolBar {
        id: toolBar
        height: 40; anchors.bottom: parent.bottom; width: parent.width; opacity: 0.9
        button1Label: "Update";
        onButton1Clicked: rssModel.reload()
    }

    function createSchema(cb)
    {
        var schema = {
            "_type": "_schemaType",
            "name": "FlickrImage",
            "schema": {
                "primaryKeys": ["imageGuid", "_type"],
                "properties": {
                    "title": {
                        "type": "string"
                    },
                    "imageGuid": {
                        "type": "string",
                    }
                }
            }
        };
        jsondb.query('[?_type="_schemaType"][?name="FlickrImage"][/_type]', function (r) {
                         if (r.data.length > 0) {
                             cb(r.data[0])
                         } else {
                             jsondb.create(schema,
                                           function (d) {
                                               console.log("created schema" + JSON.stringify(d));
                                               cb(d);
                                           },
                                           function (e) { console.log(e) });
                         }
                     });
    }

}
