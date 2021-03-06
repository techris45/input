/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QgsQuick 0.1 as QgsQuick

Item {
    id: openProjectBtn
    height: width
    property int size: width / 2
    signal activated()

    MouseArea {
        anchors.fill: parent
        onClicked: {
            openProjectBtn.activated()
            animation.restart()
        }
    }

    SequentialAnimation {
        id: animation
        running: false
        NumberAnimation {
            target: image
            property: "scale"
            from: 1
            to: 1.2
        }
        NumberAnimation {
            target: image
            property: "scale"
            from: 1.2
            to: 1
        }
    }

    Image {
        id: image
        width: size
        height: size
        anchors.centerIn: parent
        source: "project.svg"
    }
}
