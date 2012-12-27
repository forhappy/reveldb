reveldb
=======

reveldb(*RE*stful le*VELDB*) enables you to access google's [leveldb](http://code.google.com/p/leveldb/ "leveldb") in a completely RESTful way, LevelDB is a fast key-value storage library written at Google that provides an ordered mapping from string keys to string values.

Introduction
============
Reveldb provides a complete-functionality yet easy-to-use package of network interfaces to the famous key value(NOSQL) database called Leveldb, which was released and open-sourced by Google.

Reveldb to leveldb is just like [Kyoto Tycoon](http://fallabs.com/kyototycoon/ "Kyoto Tycoon") to [Kyoto Cabinet](http://fallabs.com/kyotocabinet/ "Kyoto Cabinet") or [Flare](http://labs.gree.jp/Top/OpenSource/Flare-en.html "Flare") to [Tokyo Cabinet](http://fallabs.com/tokyocabinet/ "Tokyo Cabinet"), its high performace enables you to access leveldb data in a extremely fast way through HTTP protocol. Reveldb supports both HTTP RPC and REST method to let you operate your database， includeing get, mget, set, mset, del, mdel, regex(both key and value regex matching) and many other operations(more than 40s), as a matter of fact, it's more than a network layer of leveldb, it can act as a powerful and easy-to-use database server.

Besides, reveldb also supports HTTPS protocols. In many cases you do not want expose your data to the world through HTTP request, reveldb can meet with your crucial safety needs by providing HTTPS way of accessing your data.

Installation
============
Reveldb installation is easy， it needs you no more than two minutes if you are familiar with [CMake](http://www.cmake.org/ "cmake"), the following instructions show you how to compile and install reveldb step by step

First of all, you should get reveldb source code:

> git clone git@github.com:forhappy/reveldb.git

After successfully git reveldb repository in your system, you will get a directory named "reveldb", this is where the source code lies, *cd* into it and do the following cmds:

        reveldb$ cd build
          build$ cmake ../.
          build$ make

If no errors in compilation, you will get an executable in **build** direcotry, this is what you want, just enjoy :-)

        

Features
========

High Performance
----------------
Reveldb is an event-driven NOSQL database server based on libevent, like Kyoto Tycoon and Flare, reveldb provide a high performance network layer and HTTP interfa to a key-value store leveldb, which was released by google.

Multithread Server
------------------
Reveldb is a multithreaded server, which can be more efficient on multicore CPUs.

Complete APIs
-------------
Reveldb provides more than 40 APIs,  they are classified as 5 catalogs according to the usage: Testing, Stats, Admin, CRUD and Miscs . 


HTTP RPC Support and REST Protocols 
------------------------------------
Reveldb supports both HTTP RPCs(almost finished) and REST protocols( REST protocol is under heavy developing, not available right now).


HTTPS Support
-------------
Reveldb can meet with your crucial safety needs by providing HTTPS way of accessing your data.

Master-Master and Master-Slave backups
-------------------------------------
(in heavy developing, not available right now)

Tutorial
========

Protocol
========

Testing RPCs
------------

**/rpc/void**

- Description: Do nothing, just for testing.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/void

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb RPC is healthy! :-)",
            "date": "Mon, 17 Dec 2012 12:41:04 GMT"
        }

**/rpc/head**

- Description: Act the same as /rpc/void, do nothing, just for testing.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/head

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb RPC is healthy! :-)",
            "date": "Mon, 17 Dec 2012 12:41:04 GMT"
        }


**/rpc/echo**

- Description: Echo back the input data as the output data, as well as HTTP headers, just for testing.
    
- input: (optional): arbitrary records.
   
- output: (optional): corresponding records to the input data.
    
- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/echo?db=default&key=hello&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
            "request": {
                "headers": {
                    "Host": "127.0.0.1:8088",
                    "User-Agent": "Mozilla/5.0 (X11; Ubuntu; Linux i686; rv:17.0) Gecko/20100101 Firefox/17.0",
                    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                    "Accept-Language": "en-US,en;q=0.5",
                    "Accept-Encoding": "gzip, deflate",
                    "Connection": "keep-alive"
                },
                "arguments": {
                    "db": "default",
                    "key": "hello",
                    "value": "world"
                }
            }
        }



Stats RPCs
----------

**/rpc/report**

- Description: Get the report about your reveldb server.
   
- output: (optional): arbitrary records.
    
- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/report

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb report the following information.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/status**

- Description: Get the miscellaneous status information of a database.

- input: db(optional): the database identifier.

- output: count: the number of records.

- output: size: the size of the database file.

- output: (optional): arbitrary records for other information.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/status

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb statistics.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/property**

- Description: Get the miscellaneous properties of the specified database.

- input: db: the database identifier.

- input: nfal(num-files-at-levelN, optional): the number of files at level N,
where N is an ASCII representation of a level number(e.g. "0").

- input: leveldb.stats(optional): statistics about internal operatoins of the DB.

- input: leveldb.sst(optional): describes all of the sstables that make up the db contents.

- output: miscellaneous properties of the specified database according to the input.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/property?db=default&property=leveldb.stats

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get key-value pair done.",
            "date": "Thu, 27 Dec 2012 08:57:20 GMT",
            "kv": {
                "property": "                               Compactions\nLevel  Files Size(MB) Time(sec) Read(MB) Write(MB)\n--------------------------------------------------\n  0        1        0         0        0         0\n"
            }
        }

Admin RPCs
-----------

**/rpc/new**

- Description: 

- input: db: the database identifier.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/new?db=hello

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Created new leveldb instance OK.",
            "date": "Thu, 27 Dec 2012 08:59:14 GMT"
        }

**/rpc/compact**

- Description: 

- input: db: the database identifier.

- input: start(optional): start key from which to do compaction

- input: end(optional): end key to which the compaction will stop.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/compact?db=default&start=hi

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Range compaction done.",
            "date": "Thu, 27 Dec 2012 09:01:14 GMT"
        }

**/rpc/size**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/size?start=hi&limit=hello

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get leveldb storage engine version.",
            "date": "Thu, 27 Dec 2012 09:07:45 GMT",
            "start": "hi",
            "limit": "hello",
            "size": 12324
        }

**/rpc/repair**

- Description: 

- input: db: the database identifier.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/repair

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Repair db done.",
            "date": "Thu, 27 Dec 2012 09:06:30 GMT"
        }

**/rpc/destroy**

- Description: 

- input: db: the database identifier.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/destroy?db=hello

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Destroy db done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }



CRUD RPCs
---------

**/rpc/add**

- Description: 

- input: db: the database identifier.

- input: key: key to be added.

- input: value: value along with the key to be added.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/add?db=hello&key=hello&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Set key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:11:26 GMT"
        }

**/rpc/set**

- Description: 

- input: db: the database identifier.

- input: key: key to be added.

- input: value: value along with the key to be added.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/set?db=hello&key=hello&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Set key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:11:26 GMT"
        }

**/rpc/mset**

- Description: 

- input: db: the database identifier.

- input: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mset

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb mset done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/append**

- Description: 

- input: db: the database identifier.

- input: key: key to be appended string.

- input: value: value string which will appended to the old value. 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/append?db=hello&key=hi&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Append value done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }

**/rpc/prepend**

- Description: 

- input: db: the database identifier.

- input: key: key to be prepended string.

- input: value: value string which will prepended to the old value. 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/prepend?db=hello&key=hi&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Prepend value done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }

**/rpc/insert**

- Description: 

- input: db: the database identifier.

- input: key: key to be inserted string.

- input: value: value string which will inserted to the old value. 

- input: pos: insertion position. 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/insert?db=hello&key=hi&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Insert value done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }

**/rpc/get**

- Description: 

- input: db: the database identifier.

- input: key: which key to get.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/get&db=hello&key=hello

- sample response:
        {
            "code": 200,
            "status": "OK",
            "message": "Get key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:19:05 GMT",
            "kv": {
                "hello": "worldworld"
            }
        }        

**/rpc/mget**

- Description: 

- input: db: the database identifier.

- input: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mget

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Multiple get done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/seize**

- Description: 

- input: db: the database identifier.

- input: key: which key to seize.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/seize?db=default&key=hello

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Seize key value pair OK, but note that you have just deleted the pair on reveldb server",
            "date": "Thu, 27 Dec 2012 09:21:11 GMT",
            "kv": {
                "hello": "worldworld"
            }
        }

**/rpc/mseize**

- Description: 

- input: db: the database identifier.

- input: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mseize

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Multiple seize done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/range**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/range

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }

**/rpc/regex**

- Description: 

- input: db: the database identifier.

- input: kregex: key regex.

- input: vregex: value regex.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/regex?kregex=h\*&vregex=w\* 

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:23:41 GMT",
            "kvs": [
                {
                    "hi": "10"
                }
            ]
        }       

**/rpc/kregex**

- Description: 

- input: db: the database identifier.

- input: pattern: key regex pattern.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/kregex?pattern=h\*

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:23:41 GMT",
            "kvs": [
                {
                    "hi": "10"
                }
            ]
        }       


**/rpc/vregex**

- Description: 

- input: db: the database identifier.

- input: pattern: value regex pattern.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/vregex?pattern=w\* 

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:23:41 GMT",
            "kvs": [
                {
                    "hi": "10"
                }
            ]
        }       

**/rpc/incr**

- Description: 

- input: db: the database identifier.

- input: key: key to incr.

- input: step: how long step to incr.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/incr?key=hi&step=10

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Incr value done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }

**/rpc/decr**

- Description: 

- input: db: the database identifier.

- input: key: key to decr.

- input: step: how long step to decr.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/decr?key=hi&step=10

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Decr value done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/cas**

- Description: 

- input: db: the database identifier.

- input: key: key to compare.

- input: oval: old value to be compared.

- input: nval: new value to be set.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/cas?key=hi&oval=10&nval=200

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get key-value pair done.",
            "date": "Thu, 27 Dec 2012 09:31:47 GMT",
            "kv": {
                "hi": "20"
            }
        }


**/rpc/replace**

- Description: 

- input: db: the database identifier.

- input: key: key to be replaced.

- input: value: replaced value. 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/replace?key=hi&value=world

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Replace value done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/del**

- Description: 

- input: db: the database identifier.

- input: key: key to be deleted.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/del?key=hi

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Delete key done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/mdel**

- Description: 

- input: db: the database identifier.

- input: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mdel

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Multiple delete done.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/remove**

- Description: 

- input: db: the database identifier.

- input: key: key to be removed.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/remove?key=hi

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Key removed.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/clear**

- Description: 

- input: db: the database identifier.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/clear

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Clear all keys",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }



Miscs RPCs
----------

**/rpc/sync**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/sync

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/check**

- Description: 

- input: db: the database identifier.

- input: key: key to check.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/check?key=hi

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Key exists",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/exists**

- Description: 

- input: db: the database identifier.

- input: key: Key to check.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/exists?key=hi

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Key exists.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/version**

- Description: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/version

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Get leveldb storage engine version.",
            "date": "Thu, 27 Dec 2012 09:38:05 GMT",
            "major": 1,
            "minor": 7
        }

License
=======
Copyright (c) 2012-2013 Fu Haiping haipingf AT gmail DOT com

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301 USA
