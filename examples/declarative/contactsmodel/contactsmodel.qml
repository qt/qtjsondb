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
import QtJsonDb 1.0 as JsonDb
import "content"

Item {
    width: 800
    height: 400
    id: topLevelItem

    property var nokiaPartition;
    property var nokiaPartition2;
    property var objects : [];

    JsonDb.Partition {
        id: systemPartition
        name: "com.nokia.qtjsondb.System"
    }

    JsonDb.Query{
        id: contactsQuery
        query: '[?_type="MyContacts"]'
        partition: nokiaPartition
        onResultsReady: {
            console.log("Results Available :"+resultsAvailable);
            objects = objects.concat(contactsQuery.takeResults());
            console.log("Length :" + objects.length);

        }
        onFinished: {
            console.log("Length :" + objects.length);
            for (var i = 0; i < objects.length; i++) {
                console.log("["+i+"] : "+ objects[i].firstName);
            }

        }
        onError: {
            console.log("Error: Code = " + code + " Message = " + message);
        }
    }

    JsonDb.ChangesSince {
        id: contactsChanges
        types: ["MyContacts"]
        partition: nokiaPartition
        stateNumber: 1
        onResultsReady: {
            console.log("Results Available :"+resultsAvailable);
        }
        onFinished: {
            var results = contactsChanges.takeResults();
            console.log("Length :" + results.length);
            for (var i = 0; i < results.length; i++) {
                var after = results[i].after;
                var before = results[i].before;
                if (!after.hasOwnProperty("_uuid"))
                    console.log("results["+i+"] :  object deleted " + before._uuid);
                else if (!before.hasOwnProperty("_uuid"))
                    console.log("results["+i+"] :  object added " + after._uuid);
                else
                    console.log("results["+i+"] :  object changed " + after._uuid);
            }
        }
        onError: {
            console.log("Error: Code = " + code + " Message = " + message);
        }
    }

    JsonDb.JsonDbSortingListModel {
        id: contacts
        query: '[?_type="MyContacts"]'
        roleNames: ["firstName", "lastName", "_uuid", "_version"]
        sortOrder:"[/firstName]"
    }

    // Logs notifications of type "MyContacts" in partition "com.nokia.shared"
    JsonDb.Notification {
        id:allNotifications
        query: '[?_type="MyContacts"]'
        actions: [JsonDb.Notification.Create, JsonDb.Notification.Update, JsonDb.Notification.Remove]
        onNotification: {
            if (action === JsonDb.Notification.Create) {
                console.log("New object " + result._uuid + " created");
            }
            if (action === JsonDb.Notification.Update) {
                console.log("Object " + result._uuid + " was updated");
            }
            if (action === JsonDb.Notification.Remove) {
                console.log("Object " + result._uuid + " was removed");
            }
        }
        onError: {
            console.log("Error: Code = " + code + " Message = " + result.message);
        }
    }

    function partitionCreateCallback(error, meta, response) {
        console.log("Partitions Created # "+ response.length);
        nokiaPartition  = JsonDb.partition("com.nokia.shared", topLevelItem);
        nokiaPartition2  = JsonDb.partition("com.nokia.shared2", topLevelItem);
        contacts.partitions = [nokiaPartition, nokiaPartition2];
        allNotifications.partition = nokiaPartition;
        allNotifications.enabled = true;
    }

    function checkForPartitions(error, result) {
        if (error) {
            console.log("Failed to list Partitions");
        } else {
            // result is an array of objects describing the know partitions
            var foundNokiaPartition = false;
            var foundNokiaPartition2 = false;
            for (var i = 0; i < result.length; i++) {
                console.log("["+i+"] : "+ result[i].name);
                if (result[i].name === "com.nokia.shared") {
                    foundNokiaPartition = true;
                } else if (result[i].name === "com.nokia.shared2") {
                    foundNokiaPartition2 = true;
                }
            }
            var partitionList = new Array();
            var idx = 0;
            if (!foundNokiaPartition) {
                partitionList[idx] = {_type :"Partition", name :"com.nokia.shared"};
                idx++;
            }
            if (!foundNokiaPartition2) {
                partitionList[idx] = {_type :"Partition", name :"com.nokia.shared2"};
                idx++;
            }
            if (idx>0) {
                console.log("Creating partitions");
                systemPartition.create(partitionList, partitionCreateCallback);
            } else {
                partitionCreateCallback(false, {count:0}, {});
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
        function createCallback(error, meta, response) {
            console.log("Response from create");
            console.log("meta.id = "+meta.id +" count = "+response.length);
            for (var i = 0; i < response.length; i++) {
                console.log("response._uuid = "+response[i]._uuid +" ._version = "+response[i]._version);
            }

        }
        function findCallback(error, meta, response) {
            console.log("findcallback")
            if (error) {
                // communication error , Todo: define the error value, ideally {status: htmlCode, message: "plain text" }
            } else {
                // response an array of objects matching the query and options.
                // meta contains the total number of items retrieved
                console.log("Total items = " + response.length)
                for (var i = 0; i < response.length; i++) {
                    console.log("response["+i+"] : "+ response[i]._uuid +" "+
                                response[i].firstName+" "+response[i].lastName);
                }

            }
        }

        onClicked: {
            var firstNames = ["Malcolm", "Zoe", "Hoban", "Inara", "Jayne", "Kaylee", "Simon", "River", "Shepard"]
            var lastNames = ["Reinolds", "Washburn", "Serra", "Cobb", "Frye", "Tam", "Book"]
            function rand(n) { return Math.floor(Math.random() * n); }

            var firstName = firstNames[rand(firstNames.length)]
            var lastName = lastNames[rand(lastNames.length)]
            nokiaPartition.create({ "_type":"MyContacts", "firstName":firstName, "lastName":lastName }, createCallback);
            nokiaPartition2.create({ "_type":"MyContacts", "firstName":lastName, "lastName":firstName }, createCallback);
            //nokiaPartition2.find('[?_type="MyContacts"]', findCallback);
            //contactsQuery.exec();
            contactsChanges.exec();
        }
    }
    Button {
        id: buttonDelete
        anchors.top: parent.top
        anchors.left: buttonAdd.right
        width: parent.width/5
        text: "Delete item"

        function removeCallback(error, meta, response) {
            console.log("Response from remove");
            console.log("meta.id = "+meta.id +" count = "+response.length);
            console.log("response._uuid = "+response[0]._uuid +" ._version = "+response[0]._version);
        }

        onClicked: {
            var nokiaDb = contacts.getPartition(listView.currentIndex);
            nokiaDb.remove({"_uuid":contacts.get(listView.currentIndex, "_uuid"),
                               "_version":contacts.get(listView.currentIndex, "_version"),
                           }, removeCallback)
        }
    }


    Button {
        id: buttonUpdateFName
        anchors.top: parent.top
        anchors.left: buttonDelete.right
        width: parent.width/5
        text: "Update firstName"
        function updateCallback(error, meta, response) {
            console.log("Response from update");
            console.log("meta.id = "+meta.id +" count = "+response.length);
            console.log("response._uuid = "+response[0]._uuid +" ._version = "+response[0]._version);
        }

        onClicked: {
            //var nokiaDb = contacts.getPartition(listView.currentIndex);
            //var item = contacts.get(listView.currentIndex);
            //var list = contacts.get(listView.currentIndex);

            var item = contacts.get(listView.currentIndex);
            item.object.firstName = item.object.firstName+ "*";
            item.partition.update(item.object, updateCallback);
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
                text: firstName + ", " + lastName
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
        anchors.bottom: parent.bottom
        width: parent.width
        height: 20
        color:  "lightgray"
        Text {
            anchors.centerIn: parent
            text: "rowCount : " + contacts.rowCount + "  state : " + contacts.state
        }
    }
}
