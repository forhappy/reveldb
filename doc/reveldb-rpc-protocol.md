Protocol
========
This section describes a more detailed protocol specifications of Reveldb.

Remote Procedure Calls
======================
Reveldb provides various procedues, they are classified as 5 catalogs according their usage:
Testing RPCs, Stats RPCs, Admin RPCs, CRUD PRCs and Miscs RPCs. 

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
