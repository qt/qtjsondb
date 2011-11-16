import QtQuick 2.0
import QtAddOn.JsonDb 1.0


Rectangle {
    width: 360
    height: 360

    property int fontsize: 20

    JsonDb {
        id: jsondb
    }

    function errorcb(e)
    {
        console.error('Error: ' + e);
        text.text = 'Error: ' + e;
    }

//! [Creating a Simple Index Object]
    function createIdentifierIndex(cb)
    {
        // declares an index on the "identifier" property of any object
        var indexDefinition = {
            "_type": "Index",
            "name": "identifier",
            "propertyName": "identifier",
            "propertyType": "string"
        };
        jsondb.create(indexDefinition, cb, errorcb);
    }
//! [Creating a Simple Index Object]

//! [Creating an Index Using a Property Function]
    function create(cb)
    {
        // declares an index on the "identifier" property of any object
        var indexDefinition = {
            "_type": "Index",
            "name": "identifierUppercase",
            "propertyFunction": (function (o) {
                if (o.identifier) {
                    var id = o.identifier;
                    jsondb.emit(id.toUpperCase());
                }
            }).toString(),
            "propertyType": "string"
        };
        jsondb.create(indexDefinition, cb, errorcb);
    }
//! [Creating an Index Using a Property Function]
}
