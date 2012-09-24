/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
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

Rectangle {
    width: 800
    height: 400

    property var nokiaPartition;
    property var nokiaPartition2;

    JsonDb.Partition {
        id: systemPartition
    }

    JsonDb.JsonDbSortingListModel {
        id: contacts
        query: '[?_type="MyContacts"]'
        roleNames: ["firstName", "lastName", "_uuid", "_version", "_type", "Header"]
        sortOrder:"[/firstName]"
        propertyInjector: function (item) {
            return { 'Header' : item.firstName+"-Section" };
        }
    }
    function customFunction(item) {
        return { 'Header' : item.firstName+"-$$$$Section" };
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
                nokiaPartition  = JsonDb.partition("com.nokia.shared");
                nokiaPartition2  = JsonDb.partition("com.nokia.shared2");
                contacts.partitions = [nokiaPartition, nokiaPartition2];
            }
        }
    }

    Component.onCompleted: {
        JsonDb.listPartitions(checkForPartitions);
    }

    Button {
        id: buttonAdd
        anchors.top: parent.top
        width: parent.width/4
        text: "Add contact"
        function createCallback(error, response) {
            if (error) {
                console.log("Create Error :"+JSON.stringify(error));
                return;
            }
            console.log("Response from create");
            console.log("response.id = "+response.id +" count = "+response.items.length);
            for (var i = 0; i < response.items.length; i++) {
                console.log("_uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
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
        }
    }
    Button {
        id: buttonDelete
        anchors.top: parent.top
        anchors.left: buttonAdd.right
        width: parent.width/5
        text: "Delete item"

        function removeCallback(error, response) {
            if (error) {
                console.log("Remove Error :"+JSON.stringify(error));
                return;
            }
            console.log("Response from remove");
            console.log("response.id = "+response.id +" count = "+response.items.length);
            for (var i = 0; i < response.items.length; i++) {
                console.log("_uuid = "+response.items[i]._uuid );
                console.log(JSON.stringify(response.items[i]));
            }
        }
        onClicked: {
            var item = contacts.get(listView.currentIndex);
            item.partition.remove(item.object, removeCallback);
            return;
        }
    }


    Button {
        id: buttonUpdateFName
        anchors.top: parent.top
        anchors.left: buttonDelete.right
        width: parent.width/5
        text: "Update firstName"
        function updateCallback(error, response) {
            if (error) {
                console.log("Update Error :"+JSON.stringify(error));
            }
            console.log("Response from Update");
            console.log("response.id = "+response.id +" count = "+response.items.length);
            for (var i = 0; i < response.items.length; i++) {
                console.log("_uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
            }
        }

        onClicked: {
            var item = contacts.get(listView.currentIndex);
            item.object.firstName = item.object.firstName+ "*";
            item.partition.update(item.object, updateCallback);
            contacts.propertyInjector = customFunction;
        }
    }

    Component {
        id: sectionHeading
        Rectangle {
            width: parent.width
            height: 20
            color: "lightsteelblue"

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: section
                font.bold: true
                font.pixelSize: 20
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
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 ;width: 200; height: 20}
        focus: true
        section.property: "Header"
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading
        delegate: Item {
            height: 20
            width: parent.width
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: firstName + ", " + lastName+ "  " + Header
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
