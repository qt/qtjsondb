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
import "content"

Rectangle {
    width: 480
    height: 320

    property var sharedPartition;

    function createCallback(error, response) {
        if (error) {
            console.log("Create Error :" + JSON.stringify(error));
            return;
        }
    }
    function createCollationIndex1(cb)
    {
        var indexDefinition = {
            "_type": "Index",
            "name": "pinyinIndex",
            "propertyName": "firstName",
            "propertyType": "string",
            "locale" : "zh_CN",
            "collation" : "pinyin"
        };
        sharedPartition.create(indexDefinition, createCallback);
    }
    function createCollationIndex2(cb)
    {
        var indexDefinition = {
            "_type": "Index",
            "name": "strokeIndex",
            "propertyName": "firstName",
            "propertyType": "string",
            "locale" : "zh_CN",
            "collation" : "stroke"
        };
        sharedPartition.create(indexDefinition, createCallback);
    }

    JsonDb.Partition {
        id: systemPartition
    }

    JsonDb.Query {
        id: normalQuery
        query: '[?_type="MyContacts"][/pinyinIndex]'
        onFinished: {
            listView1.text = "";
            var results = normalQuery.takeResults();
            if (results.length > 0) {
                for (var i = 0; i < results.length; i++) {
                    var result = results[i];
                    for (var k in result) {
                        if (k === "firstName" || k === "fakeFirstName" || k === "lastName") {
                            listView1.text += result[k] + "\t";
                        }
                    }
                    listView1.text += "\n";
                }
            } else {
                console.log("There is no result from normalQuery!");
            }
        }
        onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Failed to query in normalQuery: " + error.code + "-"+ error.message);
        }
    }

    JsonDb.Query {
        id: indexedQuery
        query: '[?_type="MyContacts"][/strokeIndex]'
        onFinished: {
            listView2.text = "";
            var results = indexedQuery.takeResults();
            if (results.length > 0) {
                for (var i = 0; i < results.length; i++) {
                    var result = results[i];
                    for (var k in result) {
                        if (k === "firstName" || k === "fakeFirstName" || k === "lastName") {
                            listView2.text += result[k] + "\t";
                        }
                    }
                    listView2.text += "\n";
                }
            } else {
                console.log("There is no result from normalQuery!");
            }
        }
        onStatusChanged: {
            if (status === JsonDb.Query.Error)
                console.log("Failed to query in indexedQuery: " + error.code + "-"+ error.message);
        }
    }

    function partitionCreateCallback(error, response) {
        if (error) {
            console.log("Failed to create Partitions:" + JSON.stringify(error));
            return;
        }
        sharedPartition  = JsonDb.partition("com.nokia.Shared");
        normalQuery.partition = sharedPartition;
    }

    function checkForPartitions(error, result) {
        if (error) {
            console.log("Failed to list Partitions:" + JSON.stringify(error));
        } else {
            // result is an array of objects describing the know partitions
            var foundNokiaPartition = false;
            for (var i = 0; i < result.length; i++) {
                console.log("["+i+"] : "+ result[i].name);
                if (result[i].name === "com.nokia.Shared") {
                    foundNokiaPartition = true;
                    sharedPartition  = JsonDb.partition("com.nokia.Shared");
                    normalQuery.partition = sharedPartition;
                }
            }
            var partitionList = new Array();
            var idx = 0;
            if (!foundNokiaPartition) {
                partitionList[idx] = {_type :"Partition", name :"com.nokia.Shared"};
                idx++;
            }
            if (idx>0) {
                systemPartition.create(partitionList, partitionCreateCallback);
            }
        }
    }

    Component.onCompleted: {
        JsonDb.listPartitions(checkForPartitions);
    }

    Button {
        id: buttonCreate
        anchors.top: parent.top
        anchors.left: parent.left
        width: parent.width/2
        text: "Create contacts"
        onClicked: {
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u4e00", "lastName":"1-Yi" }, createCallback);
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u4e8c", "lastName":"2-Er" }, createCallback);
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u4e09", "lastName":"3-San" }, createCallback);
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u82b1", "lastName":"4-Hua" }, createCallback);
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u9489", "lastName":"5-Ding" }, createCallback);
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u516d", "lastName":"6-Liu" }, createCallback);
            sharedPartition.create({ "_type":"MyContacts", "firstName":"\u5b54", "lastName":"7-Kong" }, createCallback);
            createCollationIndex1(createCallback);
            createCollationIndex2(createCallback);
        }
    }
    Button {
        id: buttonQuery
        anchors.top: parent.top
        anchors.right: parent.right
        width: parent.width/2
        text: "Query"
        onClicked: {
            normalQuery.partition = sharedPartition;
            normalQuery.start();
            indexedQuery.partition = sharedPartition;
            indexedQuery.start();
        }
    }

    TextEdit {
        id: listView1
        font.pixelSize: 12
        anchors.top: buttonCreate.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: parent.width/2
    }
    TextEdit {
        id: listView2
        font.pixelSize: 12
        anchors.top: buttonQuery.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: parent.width/2
    }
}