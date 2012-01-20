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

    property int fontsize: 14
    function appendLog(message)
    {
        logModel.append({"message" :message});
    }
    JsonDb.Partition {
        id: systemPartition
        name: "com.nokia.qtjsondb.System"

    }
    JsonDb.Query {
        id:schemaTypeQuery
        partition:systemPartition
        query: '[?_type="_schemaType"][?name="ContactLogView"]'
        onFinished: {
            var results = schemaTypeQuery.takeResults();
            if (results.length > 0) {
                // Schema already exists.
                installMap(false, {}, results);
            } else {
                // Create the schema
//! [Installing the View Schema]
                var schema = {"_type": "_schemaType", "name": "ContactLogView", "schema": {"extends": "View"}};
                systemPartition.create(schema, installMap);
//! [Installing the View Schema]
                var indexDefinition = {
                    "_type": "Index",
                    "name": "number",
                    "propertyName": "number",
                    "propertyType": "string"
                };
//! [Installing the Join Object]
                systemPartition.create(indexDefinition);
//! [Installing the Join Object]
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
        query: '[?_type="Map"][?targetType="ContactLogView"]'
        onFinished: {
            var results = mapTypeQuery.takeResults();
            if (results.length > 0) {
                // Map object exists, set partition
                console.log("Map Created");
                contacts.partition = systemPartition;
            } else {
                systemPartition.create(createJoinDefinition(), installMap);
            }
        }
        onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Failed to query Map " + error.code + " "+ error.message);
        }
     }

//! [Creating a Join Object]
    function createJoinDefinition()
    {
        var joinDefinition = {
            "_type": "Map",
            "targetType": "ContactLogView",
            "join": {
                "MyContact": function(contact, context) {
                    if (!context) {
                        // someone has updated contact, let's find matching callLog
                        jsondb.lookup({index: "number", value: contact.number, objectType: "CallLog"},
                                      {name: contact.name});
                        console.log("Look CallLog for "+  contact.number)
                    } else {
                        // someone has updated callLog and passes us a context
                        console.log("Found Call log"+contact.name+context.number);
                        jsondb.emit({name: contact.name, date: context.date, number: context.number});
                    }
                }.toString(),
                "CallLog": function(callLog, context) {
                    if (!context) {
                        // someone has updated a callLog, let's find matching contacts
                        jsondb.lookup({index: "number", value: callLog.number, objectType: "MyContact"},
                                      {date: callLog.date, number: callLog.number});
                        console.log("Look MyContact for "+  callLog.number)
                    } else {
                        // someone has updated contact and passes us a context
                        console.log("Found MyContact log"+context.name+callLog.number);
                        jsondb.emit({name: context.name, date: callLog.date, number: callLog.number});
                    }
                }.toString()
            }
        };
        return joinDefinition;
    }
//! [Creating a Join Object]

    function installMap(error, meta, response)
    {
        console.log("Creating reduce");
        if (error) {
            console.log("Error " + response.status + " " + response.message);
            return;
        }
        mapTypeQuery.start();
    }
    Component.onCompleted: { schemaTypeQuery.start(); }

    JsonDb.JsonDbListModel {
        id: contacts
        query: '[?_type="ContactLogView"][/date]'
        roleNames: ["date", "name", "number"]
    }

    Rectangle {
        id: buttonAddContact
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
                var firstNames = ["Malcolm", "Zoe", "Hoban", "Inara", "Jayne", "Kaylee", "Simon", "River", "Shepard" ,
                                  "Reinolds", "Washburn", "Serra", "Cobb", "Frye", "Tam", "Book"]
                function rand(n) { return Math.floor(Math.random() * n); }

                var firstName = firstNames[rand(firstNames.length)]
                var obj = { "_type":"MyContact", "name":firstName, "number":rand(100).toString()};
                systemPartition.create(obj,
                                       function (error, meta, response) {
                                           if (error) {
                                               console.log("Error " + response.status + " " + response.message);
                                               return;
                                           }
                                           appendLog(JSON.stringify(obj));
                                      });
            }
        }
    }
    Rectangle {
        id: buttonAddCall
        anchors.top: parent.top
        anchors.left: buttonAddContact.right
        anchors.margins: 2
        width: parent.width/2
        height: 50
        color: 'gray'
        border.color: "black"
        border.width: 5
        radius: 10
        Text {
            anchors.centerIn: parent
            text: "New Call"
            font.pointSize: fontsize
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                var today = new Date();
                function rand(n) { return Math.floor(Math.random() * n); }
                var obj = { "_type":"CallLog", "date":today.toString(), "number":rand(100).toString()};
                systemPartition.create(obj,
                                       function (error, meta, response) {
                                           if (error) {
                                               console.log("Error " + response.status + " " + response.message);
                                               return;
                                           }
                                           appendLog(JSON.stringify(obj));
                                      });
            }
        }
    }

    ListView {
        id: listView
        anchors.top: buttonAddContact.bottom
        anchors.bottom: logRect.top
        anchors.topMargin: 10
        anchors.bottomMargin: 10
        width: parent.width
        model: contacts
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 ;width: 200;}
        focus: true
        delegate: Row {
            spacing: 10
            Text {
                text: name + ", " + number + ", " +date
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
        id: logRect
        anchors.bottom: parent.bottom
        width: parent.width
        height: parent.height/2
        color:  "lightgray"
        ListModel {
            id:logModel
        }
        ListView {
            anchors.fill: parent
            anchors.margins: 5
            model: logModel
            header: Text {text:"Log of Created Objects"}
            delegate: Text {
                font.pointSize: 8
                text: message
            }
        }
    }
}
