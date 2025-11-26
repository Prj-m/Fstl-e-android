import QtQuick 6.0
import QtQuick.Window 6.0

Window {
    visible: true
    width: 600
    height: 400
    title: "Success Check"
    color: "#00FF00" // Set the background to bright GREEN

    Rectangle {
        width: 200
        height: 100
        anchors.centerIn: parent
        color: "blue"
        Text {
            anchors.centerIn: parent
            text: "WORKING"
            color: "white"
            font.pixelSize: 24
        }
    }
}
