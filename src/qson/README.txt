QSON as serialization

Design goals

   * Provide a binary representation of JSON, including objects and arrays
   * One data format for disk, wire, and memory
   * Must be able to stream (i.e. a query should not be forced to wait for completion before data can be returned)
   * Should be iterable (i.e. no complete deserialization necessary)
   * Provide a document extension to objects to easy JsonDB's handling of such

Core philosophy

Everything is split into packets, there are fixed-sized packets and variable-sized packets

FIXEDPACKET ::= "QSN" char(type) // fixed size implicitly defined by char(type)
VARIABLEPACKET ::= "Q" char(type) uint16(totalSizeOfPacket) // variable size

Either way, a packet is no more than 64k, the payload is limited to 64k-4bytes.

The current implementation limits strings to 30000 utf-16 encoded characters (60000 bytes). The encoding is chosen to minimize data conversions in the common client-side use cases e.g. javascript and Qt which both use UTF-16 for strings. In the future we might also enforce normalization form.

In the case of object attributes, key and value may be split across pages, where the
limit applies to key and value individually.

Qson Serialization

The serialization is always little endian!

DOCUMENT_HEADER ::=
    "QSND"
    version // storing the last version, 22 bytes
    uuid // storing the documents UUID, 18 bytes

DOCUMENT_FOOTER ::=
    "QSNd"
    version // storing the current version, 22 bytes

OBJECT_HEADER ::=
    "QSNO"

OBJECT_FOOTER ::=
    "QSNo"

META_HEADER ::= // can only appear directly following a DOCUMENT_HEADER
    "QSNM"

META_FOOTER ::=
    "QSNm"

LIST_HEADER ::=
    "QSNL"

LIST_FOOTER ::=
    "QSNl"

ARRAY_VALUE ::=
    "QA'
    uint16(sizeofPacket > 4)
    array

KEY_VALUE ::=
    "QV'
    uint16(sizeofPacket > 4)
    keyValuePairs

value ::=
        null
    ||  false
    ||  true
    ||  double
    ||  uint64
    ||  int64
    ||  string
    ||  version                     // reported as string
    ||  uuid                        // reported as string
    ||  complex value               // see next section

null    ::= "\x10\x00"
false   ::= "\x20\x00"
true    ::= "\x21\x00"
double  ::= "\x30\x00" IEEE 754-2008 (64bits)
uint64  ::= "\x31\x00" unsigned 64bits
int64   ::= "\x32\x00" 64bits
string  ::= "\x40\x00" ustring
key     ::= "\x41\x00" ustring
version ::= "\x42\x00" unsigned 32bits (updateCount) bin128(md5 hash)
uuid    ::= "\x43\x00" bin128(uuid)

ustring ::= uint16 UTF16 // must fit into one page
array ::= value array || ""
keyValuePairs ::= key value keyValuePairs || ""

Complex values in arrays and objects

   * JSON allows objects and arrays as values
   * In Qson, they are added unmodified
   * I.e. the previous page ends just before the complex value and is followed by the complex value's pages

Example

{ "hello": ["dear", "world"]}

serializes to (spaces, returns, and " just for readability)

"QSND" // object header
"QV\x12\x00" // key value page, size=18
"\x41\x00" "\x0a \x00" "h \x00 e \x00 l \x00 l \x00 o \x00" // key "hello"
"QSNL" // list header
ARRAY_VALUE (size = 31)
"QA\x19\x00" // array value page, size=25
"\x40\x00" "\x08 \x00" "d \x00 e \x00 a \x00 r \x00" // value "dear"
"\x40\x00" "\x0a \x00" "w \x00 o \x00 \x00 l \x00 d \x00" // value "world"
"QSNl" // list footer
"QSNo" // object footer

JsonDB Documents

A document is an atomic value in JsonDB.

A document is a JSON object, however some attributes have a special behavior.

<verbatim>
{
    "_uuid": "" // unique identifier of object
    "_lastVersion": "" // update count and md5 of last revision
    "_version": "" // update count and md5 of this revision

    "_meta": {
        // non-persistent information, not part of the hash
        // expected to be dropped before persisting in the database

        // defined elements:
        "ancestors": ["version1",...,"versionX"] // ancestor versions of this version
        "conflicts": [{}, {}, {}] // conflicting version (i.e. introduced through replication)
    }

    // content
}
</verbatim>

Consequently, the keys "_uuid", "_lastVersion", "_version", and "_meta" are reserved, they can only be present in a document.

In Qson serialization, their values are at fixed positions and their keys are implied.

Hashing algorithm of documents

   * The documents header and footer are skipped
   * The documents' meta object is skipped, i.e. it's existence and content does not modify the hash
   * To allow this, the _meta object (if present) must be the very first page after the document's header
   * The _meta object's key is implied by convention (otherwise a KEY_VALUE page would be required altering the hash)
   * All other pages are hashed
   * However ARRAY_VALUE and KEY_VALUE are hashed w/o the first 7 bytes to factor out different paging

Versioning algorithm of documents

   * Blank documents are created with update count and hash set to 0
   * On computing a version, its hash is compared to the last version's hash
   * If and only if they differ, the update count is increased by one.
   * If the last version is blank, it is set to the version

UUID generation algorithm

   * If the document contains a string attribute "_id", a deterministic UUID version 3 is generated from it (see RFC 4122)
   * "_id" is assumed to be a URI, but not enforced (see RFC 3305)
   * The UUID version 3 namespace is 6ba7b811-9dad-11d1-80b4-00c04fd430c8 (as defined by RFC 4122)
   * As same UUID implies same object, it is the user's responsibility to avoid clashes in the _id value
   * Otherwise, a random UUID version 4 is generated. For slow machines, a version 1 is acceptable.

Conflict management

   * Documents are identified by their UUID, same UUID is *always* same document.
   * Version numbers are deterministic (as described)
   * Each document tracks its ancestors in doc._meta.ancestors[] // encoded as "version", not "ustring"
   * Each document tracks known conflicts in doc._meta.conflicts[] // full objects, less _meta
   * Versions can be totally ordered by comparing their update count, then string comparing their hash

On merging two documents:

   * Live versions are the two documents and all of their conflicts
   * A version cannot be live, if its version is contained in the ancestor list of either document
   * If no live version is left after removing the ancestors, it is considered an attack and the merge is refused
   * The live version with the highest version number is considered the winner
   * All losers are stripped of their _meta and put into winner._meta.conflicts[]
   * All live _lastVersion are set to their respective version, all "lost" _lastVersion are added in winner._meta.ancestors[]
   * If either winner._meta.ancestor or winner._meta.conflicts is empty, it should not be added
   * "Deleting" a version is merging with a version where "_type" is set to "Tombstone" by convention
   * A losing tombstone is removed from the conflict list
   * Any document can be handed out without _meta included, expect in the case of replication (as its merged by the receiver)

Schema Uniqueness

Some schemas may define uniqueness for its instances. In JsonDB, this is mapped to UUID uniqueness. I.e. on writing such a document, the database will generate a deterministic UUID out of the unique attributes.

Note: Ideally, this is implemented as generating and setting an "_id" value, as this would keep the UUID stable, even if any processor without schema awareness decides to generate a new UUID.
