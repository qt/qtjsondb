/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: http://www.qt-project.org/
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
    JsonDb.Query {
        id:schemaTypeQuery
        partition:systemPartition
        query: '[?_type="_schemaType"][?name="PhoneView"]'
        onFinished: {
            var results = schemaTypeQuery.takeResults();
            if (results.length > 0) {
                // Schema already exists.
                createMap(false, {}, results);
            } else {
                // Create the schema
                var schema = {"_type": "_schemaType", "name": "PhoneView", "schema": {"extends": "View"}};
                systemPartition.create(schema, createMap);
            }
        }
        onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Failed to query Schema " + error.code + " "+ error.message);
        }
     }

    JsonDb.Query {
        id:mapTypeQuery
        partition:systemPartition
        query: '[?_type="Map"][?targetType="PhoneView"]'
        onFinished: {
            var results = mapTypeQuery.takeResults();
            if (results.length > 0) {
                // Map object exists, set partition
                contacts.partition = systemPartition;
            } else {
//! [Creating a Map Object]
                var mapDefinition = { "_type": "Map", "targetType": "PhoneView",
                    "map": {
                        "Contact":
                            (function (c) {
                                 for (var i in c.phoneNumbers) {
                                     var info = c.phoneNumbers[i];
                                     jsondb.emit({"phoneNumber": info, "firstName": c.firstName, "lastName": c.lastName});
                                 }
                             }
                             ).toString()
                    }
                };
//! [Creating a Map Object]
//! [Installing the Map Object]
                systemPartition.create(mapDefinition, createMap);
//! [Installing the Map Object]
            }
        }
        onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Failed to query Map " + error.code + " "+ error.message);
        }
     }

    function createMap(error, response)
    {
        if (error) {
            console.log("Error " + error.code+ " " + error.message);
            return;
        }
        mapTypeQuery.start();
    }

    JsonDb.JsonDbListModel {
        id: contacts
        query: '[?_type="PhoneView"][/phoneNumber]'
        roleNames: ["phoneNumber", "firstName", "lastName"]
        limit: 100
    }
    Component.onCompleted: { schemaTypeQuery.start(); }
    Rectangle {
        id: buttonAdd
        anchors.top: parent.top
        anchors.margins: 2
        width: parent.width
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
                                           "lastName":lastName, "phoneNumbers": [rand(1000).toString(), rand(1000).toString()]},
                                       function (error, response) {
                                           if (error) {
                                               console.log("Error " + error.code+ " " + error.message);
                                               return;
                                           }
                                           console.log("Created Contact");
                                      });
            }
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
            font.pointSize: fontsize-4
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
            font.pointSize: fontsize-4
            color: "red"
            text: ""
        }
    }
}
