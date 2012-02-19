import QtQuick 2.0
import QtJsonDb 1.0 as JsonDb

Item {

  JsonDb.Partition {
    id: partition
  }

  function createCallback(error, response) {
    if (error)
      console.debug("Error: " + JSON.stringify(error));
    Qt.quit();
  }

  Component.onCompleted: {
    var filename = "/home/user/Pictures/test000.jpeg";
    var uuid = JsonDb.uuidFromString(filename);
    console.debug("Creating object with _uuid: " + uuid);

    partition.create({
      "_uuid" : uuid,
      "_type" : "Image",
      "filename" : filename
    }, {}, createCallback);
  }
}
