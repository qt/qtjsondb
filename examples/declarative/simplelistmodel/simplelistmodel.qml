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
import "content"

Item {
    width: 400
    height: 300

    JsonDb.Partition {
        id: systemPartition
        name: "com.nokia.qtjsondb.System"
    }
    JsonDb.Partition {
        id: nokiaPartition
        name: "com.nokia.shared"
    }

    JsonDb.JsonDbListModel {
        id: contacts
        query: '[?_type="Contact"]'
        roleNames: ["firstName", "lastName", "_uuid"]
        limit: 40
    }
    function partitionCreateCallback(error, response) {
        if (error) {
            console.log(JSON.stringify(error));
        }
        console.log("Partition Created id = "+response.id +" count = "+response.items.length);
        for (var i = 0; i < response.items.length; i++) {
            console.log("response._uuid = "+response.items[i]._uuid +" ._version = "+response.items[i]._version);
        }
        contacts.partition = nokiaPartition;
    }

    Component.onCompleted: {
        systemPartition.create({_type :"Partition", name :"com.nokia.shared"}, partitionCreateCallback)
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
            nokiaPartition.create({"_type":"Contact", "firstName":firstName, "lastName":lastName})
        }
    }

    Button {
        id: buttonDelete
        anchors.top: parent.top
        anchors.left: buttonAdd.right
        width: parent.width/4
        text: "Delete item"

        onClicked: {
            nokiaPartition.remove({"_uuid": contacts.get(listView.currentIndex, "_uuid")})
            console.log("Removed Item : " + contacts.get(listView.currentIndex, "_uuid"))
        }
    }


    Button {
        id: buttonSortFName
        anchors.top: parent.top
        anchors.left: buttonDelete.right
        width: parent.width/4
        text: "Sort firstName"

        onClicked: {
            contacts.query = ("[?_type=\"Contact\"][/firstName]")
        }
    }

    Button {
        id: buttonSortLName
        anchors.top: parent.top
        anchors.left: buttonSortFName.right
        anchors.right: parent.right
        text: "Sort lastName"

        onClicked: {
            contacts.query = ("[?_type=\"Contact\"][/lastName]")
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
            text: "limit : " + contacts.limit + "  rowCount : " + contacts.rowCount + "  state : " + contacts.state
        }
    }
}
