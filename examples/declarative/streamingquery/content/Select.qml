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

Rectangle {
    id: select

    anchors.margins: 2
    width: 150
    height: 50
    color: "#cccccc"

    border.color: "black"
    border.width: 5
    radius: 10

    property alias overlayParent: overlay.parent
    property var choices: []
    property string selectedChoice: ""

    Text {
        anchors.centerIn: parent
        text: selectedChoice
        font.pointSize: 10
    }

    MouseArea {
        anchors.fill: parent
        onClicked: overlay.visible = true
    }

    Rectangle {
        id: overlay

        anchors.fill: parent
        color: "black"
        visible: false

        ListModel {
            id: model
        }

        ListView {
            id: choicesList
            anchors.fill: parent
            model: model

            delegate: Rectangle {
                width: parent.width
                height: childrenRect.height
                color: pressed ? "#cccccc" : "#000000"

                property bool pressed: false

                Text {
                    text: model.label
                    color: "#ffffff"
                    font.pointSize: 10
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        choicesList.currentIndex = model.index;
                        overlay.visible = false;
                        selectedChoice = choices[choicesList.currentIndex]
                    }
                    onPressed: parent.pressed = true
                    onReleased: parent.pressed = false
                }
            }
        }
    }

    onChoicesChanged: {
        model.clear();
        for (var i = 0; i < choices.length; i++)
            model.append({"label" : choices[i]});
        if (choices.length)
            selectedChoice = choices[0];
    }
}
