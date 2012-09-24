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
    id: container
    width: 640
    height: 480
    color:"Silver"
    property bool createNewObject: true
    property alias content:edit.text
    signal cancelled()
    signal confirmed()
    MouseArea {
        anchors.fill:parent
    }

    Text {
        id: titleText
        anchors.left:parent.left
        anchors.leftMargin: 20
        font.pixelSize: 15
        anchors.top:parent.top
        anchors.topMargin: 10
        text: createNewObject ? "Create" :"Edit"
    }

    Rectangle {
        id: textBg
        width: parent.width - 40
        height: parent.height - 100
        anchors.top:titleText.bottom
        anchors.topMargin:10
        anchors.horizontalCenter:parent.horizontalCenter
       // color:"silver"
        border.color:"black"

        Flickable {
            id: flick

            width: parent.width -20; height: parent.height;
            anchors.horizontalCenter:parent.horizontalCenter
            contentWidth: edit.paintedWidth
            contentHeight: edit.paintedHeight
            flickableDirection:Flickable.VerticalFlick
            clip: true

            function ensureVisible(r)
            {
                if (contentX >= r.x)
                    contentX = r.x;
                else if (contentX+width <= r.x+r.width)
                    contentX = r.x+r.width-width;
                if (contentY >= r.y)
                    contentY = r.y;
                else if (contentY+height <= r.y+r.height)
                    contentY = r.y+r.height-height;
            }

            TextEdit {
                id: edit
                width: flick.width
                height: flick.height
                focus: true
                wrapMode: TextEdit.Wrap
                onCursorRectangleChanged: flick.ensureVisible(cursorRectangle)
            }
        }
    }

    Button {
        id: confirmBt
        text:createNewObject? "Create":"Update"
        anchors.bottom:parent.bottom
        anchors.right:parent.right
        width :80
        anchors.bottomMargin: 10
        anchors.rightMargin: 20
        onClicked:container.confirmed();
    }
    Button {
        id: cancelBt
        text:"Cancel"
        anchors.bottom:parent.bottom
        anchors.right:confirmBt.left
        width :80
        anchors.bottomMargin: 10
        anchors.rightMargin: 20
        onClicked:container.cancelled();
    }

}

