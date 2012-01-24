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

// Generic item with no real data

import QtQuick 2.0

Rectangle {
    id: container
    width: parent.width
    height: myColumn.height
    border.color: (hidden ? "transparent" : "#888")

    property string title : "Unknown";
    property string subtitle : "";
    property string detail : "Data";
    property bool hidden: true;
    property bool expanded: false;

    signal removeMe()
    signal editMe()
    signal watchMe()

    MouseArea {
        id: clickRegion
        anchors.fill: parent
        onClicked: { container.expanded = !container.expanded; }
    }
    Column {
        id: myColumn
        x: 2
        width: parent.width

        Item {
            width: parent.width
            height: childrenRect.height

            Text {
                id: myLabel
                text: title
                color:"darkgray"
                font.pixelSize: 14
            }

            Text {
                id: mySubtitle
                font.bold: true
                anchors { left: myLabel.right; leftMargin: 10; baseline: myLabel.baseline }
                y: 2
                text: subtitle
                font.pixelSize: 15
            }
        }

        Text {
            id: itemDetail
            color: "#444";
            text: detail
            font.pixelSize: 14
            visible: false
            width: parent.width
            height: Math.max(childrenRect.height,paintedHeight)

            Column {
                anchors.right:parent.right
                spacing: 5
                Button{
                    id: removeBt
                    text: "Remove"
                    width: 80
                    onClicked:container.removeMe()
                }
                Button{
                    id: editBt
                    text: "Edit"
                    width: 80
                    onClicked:container.editMe()
                }
                Button{
                    id: watchBt
                    text: "Watch"
                    width: 80
                    visible:false
                    onClicked:container.watchMe()
                }
            }
        }
    }


    states: [
        State {
            name: "hidden"
            when: hidden
            PropertyChanges { target: myColumn; visible: false}
            PropertyChanges { target: container; height: 0 }
        },
        State {
            name: "expanded";
            when: expanded
            PropertyChanges { target: itemDetail; visible: true; }
            PropertyChanges { target: container; height: myLabel.height + itemDetail.height + 5 }
        }
    ]
}
