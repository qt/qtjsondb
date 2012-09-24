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
import "content"

Item {
    id: main
    width: 400
    height: 300

    property var firstNames: ["Malcolm", "Zoe", "Hoban", "Inara", "Jayne", "Kaylee", "Simon", "River", "Shepard"]
    property var lastNames: ["Reinolds", "Washburn", "Serra", "Cobb", "Frye", "Tam", "Book"]

    property string statusMsg: ""
    property string lastName: lastNameSelection.selectedChoice
    property int count: 0;

    JsonDb.Partition {
        id: systemPartition
    }

    JsonDb.Query {
        id: countQuery
        partition: systemPartition
        query: "[?_type=%type][?lastName=%lastName]"
        bindings: { "type" : "Contact", "lastName" : lastName }

        onResultsReady:  count += countQuery.takeResults().length
        onFinished: statusMsg = "All contacts counted"
    }

    Column {
        anchors { fill: parent; margins: 10 }
        spacing: 50

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            width: childrenRect.width
            height: childrenRect.height
            spacing: 25

            Select {
                id: lastNameSelection
                height: 50
                width: 200
                overlayParent: main
                choices: lastNames
            }

            Button {
                id: buttonCalculate
                anchors.top: parent.top
                text: "Count names"

                onClicked: {
                    statusMsg = "";
                    count = 0;
                    countQuery.start();
                }
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 10
                text: "Number of people with last name " + lastName + ": "
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 10
                text: count + " (push 'count names' to refresh)"
            }
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            id: buttonCreate
            text: "Add 100 contacts"

            onClicked: {
                count = 0;
                function rand(n) { return Math.floor(Math.random() * n); }
                var contacts = [];
                for (var i = 0; i < 100; i++) {
                    var firstName = firstNames[rand(firstNames.length)]
                    var lastName = lastNames[rand(lastNames.length)]
                    contacts.push({"_type":"Contact", "firstName":firstName, "lastName":lastName});
                }
                systemPartition.create(contacts, contactCreateCallback);
            }
        }
    }

    Rectangle {
        id: statusText
        anchors.bottom: parent.bottom
        width: parent.width
        height: 20
        color:  "#cccccc"
        Text {
            anchors.centerIn: parent
            text: statusMsg
        }
    }

    function contactCreateCallback(error, response) {
        if (error) {
            console.log(JSON.stringify(error));
            return;
        }

        statusMsg = "Created " + response.items.length + " contacts";
    }

    onLastNameChanged: count = 0
}
