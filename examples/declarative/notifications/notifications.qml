/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtJsonDb 1.0 as JsonDb

Item {
    width: 800
    height: 400
    id: topLevelItem

    property var nokiaPartition;
    property var nokiaPartition2;

    function appendLog(message)
    {
        logModel.append({"message" :message});
    }

    function appendLog2(message)
    {
        logModel2.append({"message" :message});
    }

    JsonDb.Partition {
        id: systemPartition
    }

    JsonDb.JsonDbSortingListModel {
        id: contacts
        query: '[?_type="MyContacts"]'
        roleNames: ["firstName", "lastName", "_uuid", "_version"]
        sortOrder:"[/firstName]"
    }

    // Logs notifications of type "MyContacts" in partition "com.nokia.shared"
    JsonDb.Partition {
        name: "com.nokia.shared"
        JsonDb.Notification {
            query: '[?_type="MyContacts"]'
            onNotification: {
                switch (action) {
                case JsonDb.Notification.Create :
                    appendLog("{_uuid :" + result._uuid + "} created");
                    break;
                case JsonDb.Notification.Update :
                    appendLog("{_uuid :" + result._uuid + "} was updated");
                    break;
                case JsonDb.Notification.Remove :
                    appendLog("{_uuid :" + result._uuid + "} was removed");
                    break;
                }
            }
            onStatusChanged: {
                if (status === JsonDb.Notification.Error) {
                    appendLog("Notification Error " + JSON.stringify(error));
                }
            }
        }
    }

    // Logs notifications of type "MyContacts" in partition "com.nokia.shared2"
    function onNotification(result, action, stateNumber)
    {
        switch (action) {
        case JsonDb.Notification.Create :
            appendLog2("{_uuid :" + result._uuid + "} created");
            break;
        case JsonDb.Notification.Update :
            appendLog2("{_uuid :" + result._uuid + "} was updated");
            break;
        case JsonDb.Notification.Remove :
            appendLog2("{_uuid :" + result._uuid + "} was removed");
            break;
        }
    }

    function checkForPartitions(error, result) {
        if (error) {
            console.log("Failed to list Partitions");
        } else {
            var validPartitions = 0;
            // result is an array of objects describing the know partitions
            for (var i = 0; i < result.length; i++) {
                if (result[i].name === "com.nokia.shared" || result[i].name === "com.nokia.shared2") {
                    validPartitions++;
                }
            }
            if (validPartitions != 2) {
                console.log("!!!!!!! No valid partitions found !!!!!!!!!!!");
                console.log("Error : Partitions for this example are not available");
                console.log("Run jsondb daemon in examples/declarative directory to load partions.json");
            } else {
                nokiaPartition  = JsonDb.partition("com.nokia.shared", topLevelItem);
                nokiaPartition2  = JsonDb.partition("com.nokia.shared2", topLevelItem);
                contacts.partitions = [nokiaPartition, nokiaPartition2];
                // Watch for MyContact objects in 'com.nokia.shared2'
                var createNotification = nokiaPartition2.createNotification('[?_type="MyContacts"]', topLevelItem);
                createNotification.notification.connect(onNotification);
                // Set log area title
                logRect.title = "Notifications  from: " + nokiaPartition.name;
                logRect2.title = "Notifications  from: " + nokiaPartition2.name;
            }
        }
    }

    Component.onCompleted: {
        JsonDb.listPartitions(checkForPartitions, topLevelItem);
    }

    Button {
        id: buttonAdd
        anchors.top: parent.top
        width: parent.width/4
        text: "Add contact"
        onClicked: {
            var firstNames = ["Malcolm", "Zoe", "Hoban", "Inara", "Jayne", "Kaylee", "Simon", "River", "Shepard"]
            var lastNames = ["Reinolds", "Washburn", "Serra", "Cobb", "Frye", "Tam", "Book"]
            function rand(n) { return Math.floor(Math.random() * n); }

            var firstName = firstNames[rand(firstNames.length)]
            var lastName = lastNames[rand(lastNames.length)]
            nokiaPartition.create({ "_type":"MyContacts", "firstName":firstName, "lastName":lastName });
            nokiaPartition2.create({ "_type":"MyContacts", "firstName":lastName, "lastName":firstName });
        }
    }
    Button {
        id: buttonDelete
        anchors.top: parent.top
        anchors.left: buttonAdd.right
        width: parent.width/5
        text: "Delete item"

        onClicked: {
            var nokiaDb = contacts.getPartition(listView.currentIndex);
            nokiaDb.remove({"_uuid":contacts.get(listView.currentIndex, "_uuid"),
                               "_version":contacts.get(listView.currentIndex, "_version"),
                           })
        }
    }


    Button {
        id: buttonUpdateFName
        anchors.top: parent.top
        anchors.left: buttonDelete.right
        width: parent.width/5
        text: "Update firstName"
        onClicked: {
            var item = contacts.get(listView.currentIndex);
            item.object.firstName = item.object.firstName+ "*";
            item.partition.update(item.object);
        }
    }

    ListView {
        id: listView
        anchors.top: buttonAdd.bottom
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
                text: firstName + ", " + lastName + "  " + _uuid
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
        property string title
        anchors.bottom: parent.bottom
        width: parent.width/2
        height: parent.height/2
        color:  "lightgray"
        ListModel {
            id:logModel
         }
        ListView {
            anchors.fill: parent
            anchors.margins: 5
            model: logModel
            header: Text {text:logRect.title}
            delegate: Text {
                font.pointSize: 8
                text: message
            }
        }
    }
    Rectangle {
        id: logRect2
        property string title
        anchors.bottom: parent.bottom
        anchors.left: logRect.right
        anchors.right: parent.right
        height: parent.height/2
        color:  "lightyellow"
        ListModel {
            id:logModel2
         }
        ListView {
            anchors.fill: parent
            anchors.margins: 5
            model: logModel2
            header: Text {text:logRect2.title}
            delegate: Text {
                font.pointSize: 8
                text: message
            }
        }
    }
}
