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

Mater-Master and Master-Slave backups
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
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
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
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/property**

- Description: Get the miscellaneous properties of the specified database.

- input: db: the database identifier.

- input: nfal(num-files-at-levelN, optional): the number of files at level N,
where N is an ASCII representation of a level number(e.g. "0").

- input: stats(optional): statistics about internal operatoins of the DB.

- input: sst(optional): describes all of the sstables that make up the db contents.

- output: miscellaneous properties of the specified database according to the input.

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/property

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }



Admin RPCs
-----------

**/rpc/new**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/new?db=hello

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/compact**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/compact

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/size**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/size

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/repair**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/repair

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/destroy**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/destroy

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }



CRUD RPCs
---------

**/rpc/add**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/add

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/set**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/set

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/mset**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mset

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/append**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/append

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/prepend**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/prepend

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/insert**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/insert

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/get**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/get

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/mget**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mget

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/seize**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/seize

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/mseize**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mseize

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
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

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/regex

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/incr**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/incr

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/decr**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/decr

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/cas**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/cas

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/replace**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/replace

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/del**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/del

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/mdel**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/mdel

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/remove**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/remove

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/clear**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/clear

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
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

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/check

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/exists**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/exists

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
        }


**/rpc/version**

- Description: 

- input: db: the database identifier.

- input: 

- output: 

- status code: 200.

- sample request:

        http://127.0.0.1:8088/rpc/version

- sample response:

        {
            "code": 200,
            "status": "OK",
            "message": "Reveldb echoed the HTTP headers and query arguments of your request.",
            "date": "Mon, 17 Dec 2012 12:50:22 GMT",
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
