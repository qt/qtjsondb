/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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
import QtJsonDb 1.0 as JsonDb


Rectangle {
    width: 360
    height: 360

    property int fontsize: 16

    JsonDb.Partition {
        id: systemPartition
        name: "com.nokia.qtjsondb.System"
    }

    function createViewSchema()
    {
        var schema = {
            "_type": "_schemaType",
            "name": "PhoneView",
            "schema": {
                "extends": "View"
            }
        };
        systemPartition.find('[?_type="_schemaType"][?name="PhoneView"][/_type]',
             function (error, meta, response) {
                 if (error) {
                     console.log("Error " + response.status + " " + response.message);
                     return;
                 }
                 if (response.length > 0) {
                     console.log("Schema exists");
                     createMap(false, {}, response);
                 } else {
                     console.log("Create Schema");
                     systemPartition.create(schema, createMap);
                 }
            });
    }

//! [Creating a Map Object]
    function createMap(error, meta, response)
    {
        console.log("Creating map");

        if (error) {
            console.log("Error " + response.status + " " + response.message);
            return;
        }

        var mapDefinition = {
            "_type": "Map",
            "targetType": "PhoneView",
            "map": {
                "Contact":
                    (function (c) {
                        for (var i in c.phoneNumbers) {
                            var info = c.phoneNumbers[i];
                            for (var k in info)
                                jsondb.emit({"phoneNumber": info[k], "firstName": c.firstName, "lastName": c.lastName});
                        }
                        }
                    ).toString()
            }
        };
//! [Creating a Map Object]

//! [Installing the Map Object]
        systemPartition.find('[?_type="Map"][?targetType="PhoneView"]',
             function (error, meta, response) {
                 if (error) {
                     console.log("Error " + response.status + " " + response.message);
                     return;
                 }
                 if (response.length > 0) {
                     console.log("Map Object exists, set partition");
                     contacts.partition = systemPartition;
                 } else {
                     console.log("Create Map Object");
                     systemPartition.create(mapDefinition, createMap);
                 }
            });
//! [Installing the Map Object]
    }

    JsonDb.JsonDbListModel {
        id: contacts
        query: '[?_type="PhoneView"][/phoneNumber]'
        roleNames: ["phoneNumber", "value"]
        limit: 100
    }

    Rectangle {
        id: buttonInit
        anchors.top: parent.top
        anchors.margins: 2
        width: parent.width/2
        height: 50
        color: 'gray'
        border.color: "black"
        border.width: 5
        radius: 10
        Text {
            anchors.centerIn: parent
            text: "Init"
            font.pointSize: fontsize
        }
        MouseArea {
            anchors.fill: parent
            onClicked: { createViewSchema(); }
        }
    }
    Rectangle {
        id: buttonAdd
        anchors.left: buttonInit.right
        anchors.top: parent.top
        anchors.margins: 2
        width: parent.width/2
        height: 50
        color: 'gray'
        border.color: "black"
        border.width: 5
        radius: 10
        Text {
            anchors.centerIn: parent
            text: "Add Contact"
            font.pointSize: fontsize
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                var firstNames = ["Malcolm", "Zoe", "Hoban", "Inara", "Jayne", "Kaylee", "Simon", "River", "Shepard"]
                var lastNames = ["Reinolds", "Washburn", "Serra", "Cobb", "Frye", "Tam", "Book"]
                function rand(n) { return Math.floor(Math.random() * n); }

                var firstName = firstNames[rand(firstNames.length)]
                var lastName = lastNames[rand(lastNames.length)]
                systemPartition.create({ "_type":"Contact", "firstName":firstName,
                                           "lastName":lastName, "phoneNumbers": [rand(1000), rand(1000)]},
                                       function (error, meta, response) {
                                           if (error) {
                                               console.log("Error " + response.status + " " + response.message);
                                               return;
                                           }
                                           console.log("Created Contact");
                                      });
            }
        }
    }

    ListView {
        id: listView
        anchors.top: buttonInit.bottom
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
                text: phoneNumber + ":   " + firstName + ", " + lastName
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
            font.pointSize: fontsize-2
            text: "Cache Limit : " + contacts.limit + "  rowCount : " + contacts.rowCount + "  state : " + contacts.state
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
