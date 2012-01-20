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
import "content/inspector.js" as InspectorDb

Item {
    id: screen
    width: 800
    height: 600


    ListModel {
        id: rootModel
    }

    Rectangle{
        id: allVisible
        width: 15
        height: 15
        anchors.bottom:divider0.top
        anchors.bottomMargin: 5
        anchors.right:allInvisible.left
        anchors.rightMargin: 10
        color:"green"
        MouseArea {
            anchors.fill:parent
            anchors.margins: -5
            onClicked: {
                for (var i = 0 ;i< rootModel.count; i++) {
                    rootModel.setProperty(i, "Visible", true);
                }
            }
        }

    }
    Rectangle{
        id: allInvisible
        width: 15
        height: 15
        anchors.bottom:divider0.top
        anchors.bottomMargin: 5
        anchors.right:divider1.left
        anchors.rightMargin: 10
        color:"red"
        MouseArea {
            anchors.fill:parent
            anchors.margins: -5
            onClicked: {
                for (var i = 0 ;i< rootModel.count; i++) {
                    rootModel.setProperty(i, "Visible", false);
                }
            }
        }

    }
    Rectangle {
        id: divider0
        width: 190
        height: 1
        anchors.left:screen.left
        anchors.leftMargin: 10
        anchors.top:parent.top
        anchors.topMargin: 40
        color:"black"
    }
    ListView {
        id: rootView
        model:rootModel

        anchors.left:parent.left
        anchors.top:divider0.bottom
        anchors.margins: 10
        //  color:"lightgray"
        clip:true
        width:200
        height:parent.height - 70
        delegate: Rectangle {
            width:parent.width
            height:title.height + 10
            Text {
                id: title
                width: parent.width - 50
                anchors.verticalCenter:parent.verticalCenter
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment:Text.AlignLeft
                anchors.leftMargin: 10
                anchors.rightMargin: 50
                text: Title
                wrapMode:Text.WrapAnywhere
                font.pixelSize: 12
            }

            Rectangle{
                id: visibleCb
                width: 15
                height: 15
                anchors.verticalCenter:parent.verticalCenter
                anchors.left:title.right
                anchors.leftMargin: 20
                property bool nVisible:  model.Visible
                color:nVisible? "green":"red"
                MouseArea {
                    anchors.fill:parent
                    anchors.margins: -5
                    onClicked: {
                        rootModel.setProperty(index, "Visible",!visibleCb.nVisible )
                    }
                }

            }
            Rectangle {
                width:parent.width - 20
                height: 1
                anchors.horizontalCenter:parent.horizontalCenter
                color:"slategray"
                anchors.bottom:parent.bottom
            }

        }
    }

    Rectangle {
        id: divider1
        width: 2
        height: parent.height - 20
        anchors.left:rootView.right
        anchors.leftMargin: 10
        anchors.verticalCenter:parent.verticalCenter
        color:"black"
    }

    Rectangle{
        id: quickSearchBox
        anchors.left:rootView.right
        anchors.right:parent.right
        anchors.top:parent.top
        anchors.margins: 20
        height: 30
        // width: parent.width - 300
        border.color:"gray"
        TextInput {
            id: searchbox
            width:parent.width
            anchors.verticalCenter:parent.verticalCenter
            font.pixelSize: 20
        }
    }

    Flickable {
        id: flickable
        anchors.left:rootView.right
        anchors.right:parent.right
        anchors.top:quickSearchBox.bottom
        height: parent.height - 250
        anchors.topMargin:10
        anchors.leftMargin: 20
        anchors.rightMargin:20
        contentHeight: myColumn.height
        clip:true
        flickableDirection:Flickable.VerticalFlick

        Column{
            id: myColumn
            width:flickable.width
            height:childrenRect.height
            Repeater {
                model:rootModel
                delegate: Item {
                    id: dinstance
                    width: flickable.width
                    height: showItems? (header.height + itemsList.height): header.height
                    property bool showItems: false
                    visible: model.Visible //&& (searchText)
                    //                    property string searchText: searchbox.text
                    //                    onSearchTextChanged: {
                    //                        if (searchText ) {
                    //                            filteredModel.clear();
                    //                            for (var i = 0; i< dModel.count ; i++) {
                    //                                var str = JSON.stringify(dModel.get(i));
                    //                                if (str.indexOf(searchText) != 0) {
                    //                                    filteredModel.append(dModel.get(i));
                    //                                }
                    //                            }
                    //                        }
                    //                    }
                    onShowItemsChanged: {
                        //                        if (showItems) {
                        //                            if (searchText) {
                        //                                itemsRepeater.model = filteredModel;
                        //                            }else{
                        //                                itemsRepeater.model = dModel;
                        //                            }
                        //                        }

                        if (!itemsRepeater.model) {
                            itemsRepeater.model = dModel;
                        }

                    }

                    Header {
                        id: header
                        width:parent.width - 1
                        title:Title
                        count: dModel.count
                        onClicked: {
                            dinstance.showItems = !dinstance.showItems;
                            //InspectorDb.headerClick(title);
                        }
                    }
                    Column {
                        id: itemsList
                        width:flickable.width
                        height: childrenRect.height

                        visible:dinstance.showItems
                        anchors.top:header.bottom
                        Repeater {
                            id:itemsRepeater
                            delegate: Generic {
                                id: instance
                                width: flickable.width-1
                                hidden: false
                                property bool viewShown: dinstance.showItems
                                title:model._type
                                subtitle : InspectorDb.findsubtitle(model)
                                detail: model.inspector_detail
                                property string searchText: searchbox.text
                                onSearchTextChanged: {
                                    if (searchText) {
                                        if (detail.indexOf(searchText)!= -1) {
                                            instance.hidden = false;
                                        }else {
                                            instance.hidden = true;
                                        }
                                    }else {
                                        instance.hidden = false;
                                    }
                                }
                                onViewShownChanged: {
                                    expanded = false;
                                }
                                onRemoveMe: {
                                    InspectorDb.removeObject(model._uuid);
                                }

                                onEditMe: {
                                    objectEditor.show = true;
                                    objectEditor.createNewObject = false;
                                    objectEditor.content = model.inspector_detail; /*JSON.stringify(d,null," ")*/;
                                }

                                Component.onCompleted: {
                                    // InspectorDb.setData(instance, model)
                                }
                            }
                        }
                    }
                    ListModel {
                        id: dModel
                    }

                    property variant notificationObj:null
                    Component.onCompleted: {
                        var queryStr = textbox.text + '[?_type="' + Title+ '"]';

                        notificationObj = JsonDb.notification(queryStr, ['create','update','remove'], function (result, action){
                                                                  if (action == 'create') {
                                                                      dModel.append(result);
                                                                  }else {
                                                                      for (var i = 0; i< dModel.count; i++) {
                                                                          if (dModel.get(i)._uuid == result._uuid) {
                                                                              if (action == 'update') {
                                                                                  result.inspector_detail = InspectorDb.getDetail(result);

                                                                                  dModel.set(i, result);
                                                                                  break;
                                                                              }else if (action == 'remove') {
                                                                                  dModel.remove(i);
                                                                                  break;
                                                                              }
                                                                          }
                                                                      }
                                                                  }
                                                              }
                                                              );

                        JsonDb.query(queryStr, function(result){
                                         var data = result.data;
                                         for (var i = 0; i< data.length; i++) {
                                             var d = data[i];
                                             d.inspector_detail = InspectorDb.getDetail(data[i]);
                                             dModel.append(d);
                                         }
                                     }
                                     )
                    }
                    Component.onDestruction: {
                        if (notificationObj) {
                            notificationObj.remove();
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: divider2
        width: flickable.width
        height: 2
        anchors.top:flickable.bottom
        anchors.topMargin:10
        anchors.right:parent.right
        anchors.rightMargin:20
        color:"black"
    }

    Rectangle {
        id: divider3
        width: flickable.width
        height: 2
        anchors.top:divider2.bottom
        anchors.topMargin:60
        anchors.right:parent.right
        anchors.rightMargin:20
        color:"black"
    }

    Item {
        id: queryBox
        // border.color:"gray"
        anchors.left:divider1.right
        anchors.leftMargin: 10
        anchors.right:parent.right
        anchors.rightMargin: 20
        anchors.top:divider2.bottom
        anchors.topMargin: 10
        anchors.bottom:divider3.top
        anchors.bottomMargin: 10
        Rectangle {
            id: cleanBt
            width: 20
            height: 20
            radius: 15
            anchors.left:textbg.right
            anchors.leftMargin: 5
            anchors.verticalCenter:parent.verticalCenter
            color:cleanMa.pressed? "gray": "silver"
            Text {
                text: "x"
                color:"red"
                anchors.centerIn:parent
                font.pixelSize: 14
            }
            MouseArea {
                id: cleanMa
                anchors.fill:parent
                onClicked: {
                    textbox.text = '';
                }
            }

        }
        Rectangle{
            id: textbg
            anchors.left:parent.left
            anchors.top:parent.top
            anchors.bottom:parent.bottom
            // anchors.margins: 10
            width: parent.width - 300
            border.color:"gray"
            TextInput {
                id: textbox
                width:parent.width
                anchors.verticalCenter:parent.verticalCenter
                font.pixelSize: 20
            }
        }

        Button {
            id:searchBt
            text:"Query"
            width: 80
            anchors.left:cleanBt.right
            anchors.leftMargin: 10
            anchors.verticalCenter:parent.verticalCenter
            onClicked: {
                InspectorDb.loaddata(textbox.text);
            }
        }

        Button {
            id:saveBt
            text:"Save query"
            width: 100
            anchors.left:searchBt.right
            anchors.leftMargin: 10
            anchors.verticalCenter:parent.verticalCenter
            onClicked: {
                saveQueryDialog.open();
                //   InspectorDb.loaddata(textbox.text);
            }
        }
        Rectangle {
            id: divider4
            width: 1
            height: saveBt.height
            color:"black"
            anchors.left:saveBt.right
            anchors.leftMargin:5
            anchors.verticalCenter:parent.verticalCenter
        }

        Button {
            id:newBt
            text:"New"
            width: 60
            anchors.left:divider4.right
            anchors.leftMargin: 5
            anchors.verticalCenter:parent.verticalCenter
            onClicked: {
                objectEditor.show = true;
                objectEditor.createNewObject = true;
            }
        }
    }


    Dialog {
        id: saveQueryDialog
        onAccepted: {
            var obj = {
                "_type":"UserCommand",
                "query":textbox.text,
                "title":nameText.text
            }
            JsonDb.create(obj);
        }

        title: [Text {
                text:"Save query as .."
                // height: 40
                width:parent.width
                verticalAlignment:Text.AlignVCenter
                horizontalAlignment:Text.AlignHCenter
                font.family: "Nokia Pure"
                font.weight:Font.Light
                font.pixelSize:20
                color: "#1C97FF"
            }
        ]
        content: [
            Item{
                width:parent.width
                height: 200
                Rectangle {
                    anchors.centerIn:parent
                    width: 400
                    height: 30
                    color:"white"
                    border.color:"black"
                    TextInput {
                        id: nameText
                        width:parent.width - 10
                        anchors.centerIn:parent
                        font.family: "Nokia Pure"
                        font.weight:Font.Light
                        font.pixelSize:13
                        // wrapMode: Text.WordWrap
                        text:"New query"
                    }
                }


            }
        ]
        buttons: [
            Row{
                anchors.horizontalCenter:parent.horizontalCenter
                spacing: 20
                Button {
                    id: saveQueryBt
                    text: "Save"
                    onClicked: saveQueryDialog.accept()
                }
                Button {
                    id: cancelBt
                    //  anchors.top: rmAlbumBt.bottom
                    //  anchors.topMargin: root.platformStyle.buttonsTopMargin
                    text: "Cancel"
                    onClicked: saveQueryDialog.close()
                }
            }
        ]
    }
    ListModel {
        id: queryCmdModel
        ListElement {
            query: '[/_type]'
            title: 'All items'
        }
        ListElement {
            query: '[?_type="MEDIAINFO"]'
            title:"All MEDIAINFO"
        }
        ListElement {
            query: '[?_type="UserCommand"]'
            title:"All commands"
        }
    }

    Flow {
        anchors.left:divider1.right
        anchors.leftMargin: 10
        anchors.top:divider3.bottom
        anchors.topMargin: 10
        anchors.right:parent.right
        anchors.rightMargin: 20
        spacing: 10

        Repeater {
            model:queryCmdModel
            delegate: Button {
                text:title
                width: 150
                onClicked: {
                    textbox.text = query;
                    InspectorDb.loaddata(textbox.text);
                }
            }
        }
    }

    Editor {
        id: objectEditor
        width: parent.width
        height :500
        //        anchors.bottom:parent.bottom
        y: parent.height
        property bool show: false
        onCancelled: {
            show = false;
        }
        onConfirmed: {
            var obj = JSON.parse(content);
            if (createNewObject) {
                if (obj._uuid) {
                    delete obj._uuid;
                }

                JsonDb.create(obj);
            }else {
                if (!obj._uuid) {
                    // console.log(" missing uuid")
                }else {
                    JsonDb.update(obj);
                }
            }
            show = false;
        }

        states: [
            State {
                name: "show"
                when: objectEditor.show
                PropertyChanges {
                    target: objectEditor
                    y: screen.height - objectEditor.height
                }
            },
            State {
                name: "hide"
                when: !objectEditor.show
                PropertyChanges {
                    target: objectEditor
                    y: screen.height
                }
            }

        ]

    }

    Component.onCompleted: {
        JsonDb.connected.connect(function() {
            InspectorDb.inspectorStarted();
        });
        JsonDb.connect({"host": "localhost", "port": 6847});
    }
}

