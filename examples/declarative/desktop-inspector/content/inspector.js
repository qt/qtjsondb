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

var componentDict = {};
var schema = {};

function makeComponent(name) {
    var component;

    if (name in componentDict) {
        component = componentDict[name];
    }
    else {
        component = Qt.createComponent(name+".qml");
        if (component.status != Component.Ready) {
            console.debug("Error creating " + name + ".qml component");
            console.debug(component.errorString());
            throw "InitComponentError";
        }
        componentDict[name] = component;
    }

    var obj = component.createObject(myColumn);
    if (obj == null) {
        console.debug("Error creating object of type " + name);
        console.debug(component.errorString());
        throw "CreationError";
    }
    return obj;
}


var mapping = {
    "image": "url",
    "_schemaType": "name",
    "CONTACT": function(obj) { return obj.NAME.given + " " + obj.NAME.family; }
};


function findsubtitle(record) {
    try {
        var list = ["identifier","name"];
        for (var i = 0; i< list.length; i++) {
            var p = list[i];
        if (record._type in mapping)
            p = mapping[record._type];

        if (typeof(p) == "string" && p in record)
            return record[p];

        if (typeof(p) == "function")
            return p(record);

        p = "name";
    }

    } catch (err) {
        console.debug(err);
    }
    return " ";
}

var appsets = {};

function headerClick(name) {
    var set = appsets[name];
    for (var i = 0 ; i < set.length ; i++) {
        set[i].hidden = !set[i].hidden;
    }
}

/*
   This is incomplete and won't parse a complete record.
   Needs to be made recursive
*/

function prettyFormat( record, schema, indent )
{
    var result = "";
    if ("title" in schema)
        result += indent + schema.title + "\n";

    if ("properties" in schema) {
        var schema_properties = schema.properties;
        for (var property in record) {
            if (property in schema_properties) {
                var schema_entry = schema_properties[property];
                var ptype = (("type" in schema_entry) ? schema_entry["type"] : "undefined");
                var title = (("title" in schema_entry) ? schema_entry["title"] : ptype);
                if (ptype == "boolean") {
                    result += indent + " " + title + ": " + (record[property]?"true":"false") + "\n";
                }
                if (ptype == "string" || ptype == "integer" || ptype == "number" || ptype == "any")
                    result += indent + " " + title + ": " + record[property] + "\n";
                else if (ptype == "array") {
                    var a = record[property];
                    if (a.length) {
                        result += indent + " " + title + ": [\n";
                        var array_schema = schema_entry["items"];
                        for (var i = 0 ; i < a.length ; i++ ) {
                            result += prettyFormat( a[i], array_schema, indent + "  ");
                            if (i < a.length - 1)
                                result += indent + " ---- ";
                        }
                        result += "]\n";
                    }
                    else {
                        result += indent + " " + title + ": []\n";
                    }
                }
                else if (ptype == "object") {
                    result += prettyFormat( record[property], schema_entry, indent + "  ");
                }
            }
        }
    }
    return result;
}


function loadSchema(next) {
//    jsondb.notification('[?_type="_schemaType"]',['create', 'remove'], function(result, action) {
//        if (action == 'create') {
//            schema[result.name] = result;
//        }else if (action == 'remove') {
//            delete schema[result.name];
//        }
//    }
//);
    JsonDb.query('[?_type="_schemaType"]',function(result) {
        var records = result.data;
        for (var i = 0; i< records.length; i++) {
            schema[records[i].name] = records[i];
        }

        if (next) {
            next();
        }
    }
    )
}

function loadQuery(next) {
    JsonDb.notification('[?_type="UserCommand"]', ['create',"update","remove"], function(result,action) {
        if (action == 'create') {
            queryCmdModel.append(result);
        } else if (action == 'remove') {
            for (var i = 0; i< queryCmdModel.count ; i++) {
                if (queryCmdModel.get(i)._uuid && queryCmdModel.get(i)._uuid == result._uuid) {
                    queryCmdModel.remove(i);
                    break;
                }
            }
        } else if ( action == 'update') {
            for (var i = 0; i< queryCmdModel.count ; i++) {
                if (queryCmdModel.get(i)._uuid && queryCmdModel.get(i)._uuid == result._uuid) {
                    queryCmdModel.set(i, result);
                    break;
                }
            }
        }
    }
);
    JsonDb.query('[?_type="UserCommand"]', function(result) {
        var cmds = result.data;
        for (var i = 0; i < cmds.length; i++) {
            queryCmdModel.append(cmds[i]);/*{"query":cmds[i].query, "title": cmds[i].title}*/
        }
        if (next) {
            next();
        }
    }
)
}

function loaddata(queryString) {
    if (!queryString) {
        queryString = '[/_type]';
    }
    JsonDb.query(queryString, function(data) {
        var records = data.data;
        var header;
        rootModel.clear();
        records.sort(function(a,b) {
            return (a._type > b._type ? 1 :
                    (a._type < b._type ? -1 : 0))});


        for (var i = 0 ; i < records.length ; i++) {
            var record = records[i];

            if (typeof(header) == "undefined" || record._type != header) {

                header = record._type;
                rootModel.append({"Title":record._type, "Visible": true});
            }
        }
    });
}

function getDetail(record) {
    var ret = JSON.stringify(record,null," ");

    if (record._type in schema) {
        try {
            ret = prettyFormat(record, schema[record._type].schema,"") +
                          "--------------------------------------------\n" + ret;

        } catch (err) {
            console.debug(err);
        }
    }
    return ret;
}


function inspectorStarted() {
    loadSchema(loadQuery(loaddata));
}

function removeObject(uuid) {
    var obj = {
        "_uuid":uuid
    }

    JsonDb.remove(obj);
}
