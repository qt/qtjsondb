/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtAddOn.JsonDb 1.0


Rectangle {
    width: 360
    height: 360

    property int fontsize: 20

    JsonDb {
        id: jsondb
    }

    function errorcb(e)
    {
        console.error('Error: ' + e);
        text.text = 'Error: ' + e;
    }

//! [Installing the View Schema]
    function installSchema(cb)
    {
        var schema = {
            "_type": "_schemaType",
            "name": "PhoneView",
            "schema": {
                "extends": "View"
            }
        };
        jsondb.query('[?_type="_schemaType"][?name="ContactLogView"][/_type]', function (r) {
                         if (r.data.length > 0) {
                             cb(r.data[0])
                         } else {
                             jsondb.create(schema, cb, function (e) { console.log(e) });
                         }
                     });
    }
//! [Installing the View Schema]

//! [Creating a Join Object]
// joinview.qml
    function createJoinDefinition()
    {
        var joinDefinition = {
            "_type": "Map",
            "targetType": "ContactLogView",
            "join": {
                "Contact": function(contact, context) {
                    if (!context) {
                        // someone has updated contact, let's find matching callLog
                        jsondb.lookup({index: "number", value: contact.number, objectType: "CallLog"},
                                      {name: contact.name});
                    } else {
                        // someone has updated callLog and passes us a context
                        jsondb.emit({name: contact.name, date: context.date, number: context.number});
                    }
                }.toString(),
                "CallLog": function(callLog, context) {
                    if (!context) {
                        // someone has updated a callLog, let's find matching contacts
                        jsondb.lookup({index: "number", value: callLog.number, objectType: "Contact"},
                                      {date: callLog.date, number: callLog.number});
                    } else {
                        // someone has updated contact and passes us a context
                        jsondb.emit({name: context.name, date: callLog.date, number: callLog.number});
                    }
                }.toString()
            }
        };
        return joinDefinition;
    }
//! [Creating a Join Object]

//! [Installing the Join Object]
    function installMap(cb)
    {
        console.log("Creating join");

        var def = createJoinDefinition();

        jsondb.query('[?_type="Map"][?targetType="ContactLogView"]', function (r) {
            if (r.data.length > 0) {
                cb(r.data[0])
            } else {
                jsondb.create(def, function (v) { cb(v); }, function (e) { console.log(e) });
            }
        });
    }
//! [Installing the Join Object]

    JsonDbListModel {
        id: contacts
        query: '[?_type="ContactLogView"][/key]'
        roleNames: ["key", "value"]
        limit: 40
    }

    Rectangle {
        id: buttonAdd
        anchors.top: parent.top
        anchors.margins: 2
        width: parent.width/4
        height: 50
        color: 'gray'
        border.color: "black"
        border.width: 5
        radius: 10
        Text {
            anchors.centerIn: parent
            text: "Update"
            font.pointSize: fontsize
        }
        MouseArea {
            anchors.fill: parent
            onClicked: { installSchema(installJoin); contacts.query = '[?_type="ContactLogView"]'; }
        }
    }

    ListView {
        id: listView
        anchors.top: buttonAdd.bottom
        anchors.bottom: statusText.top
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        width: parent.width
        model: contacts
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 ;width: 200;}
        focus: true
        delegate: Row {
            spacing: 10
            Text {
                text: key + ":   " + value.firstName + ", " + value.lastName
                font.pointSize: fontsize
                MouseArea {
                   anchors.fill: parent;
                   onPressed: {
                       listView.currentIndex = index;
                   }
                }
            }
        }
    }
    Rectangle {
        id: statusText
        anchors.bottom: messageRectangle.top
        width: parent.width
        height: 20
        color:  "lightgray"
        Text {
            anchors.centerIn: parent
            font.pointSize: fontsize
            text: "limit : " + contacts.limit + "  rowCount : " + contacts.rowCount + "  state : " + contacts.state
        }
    }
    Rectangle {
        id: messageRectangle
        anchors.bottom: parent.bottom
        width: parent.width
        height: 24
        color: "white"
        Text {
            id: messageText
            anchors.centerIn: parent
            font.pointSize: fontsize
            color: "red"
            text: ""
        }
    }
}
