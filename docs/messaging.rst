Messaging API
#############

The main point of dwork is to provide a messaging API
for clients to interact with the scheduler.

Both job producers and consumers use the same API.
Both are considered clients.
They just make different sorts of requests.

Clients send and receive `QueryMsg` protocol buffers over ZeroMQ.
These are documented in `proto/TaskMsg.proto`::

    message QueryMsg {
      enum Type {
        Create   = 0; // Create new tasks (requires TaskMsg)
        Steal    = 1; // request a ready task (requires name == hostname)
        Transfer = 2; // notify on completion of a task (requires TaskMsg)
        Lookup   = 3; // lookup a location
        NotFound = 4; // reply to steal / transfer
        Exit     = 5; // signal exit of server
        OK       = 6;
      }
      required Type    type = 1;
      repeated TaskMsg task = 2; // required when type == Notify
      optional string  name = 3; // task to lookup
      optional int32   n = 4;
      optional string  location = 6; // hostname lookup response OR hostname doing stealing
    }

The sections below detail request/reply pairs exchanged between server and client.
Components of QueryMsg not explicitly listed below must be absent from the message,
or else the message is ill-formed and will be rejected.

Multiple Requests and Responses listed indicate that any of the given
pairs could be sent or received.

All responses could also include the following:

  * **Response**

    - type = QueryMsg::Error - improperly formatted request

  * **Response**

    - type = QueryMsg::Exit - server is exiting


create task(s)
--------------

This adds a new task to the scheduler, or adds
additional dependencies to an existing task.

  * Request

    - type = QueryMsg::Create
    - task = [TaskMsg]

  * **Response**

    - type = QueryMsg::OK


steal task(s)
-------------

This asks for a ready task and assigns it to the requesting host.

  * Request

    - type      = QueryMsg::Steal
    - location  = hostname doing stealing

  * Request

    - type      = QueryMsg::Steal
    - location  = hostname doing stealing
    - n         = max number of tasks requested

  * **Response**

    - type = QueryMsg::Transfer
    - task = [TaskMsg]

  * **Response**

    - type = QueryMsg::NotFound - server has no ready work


complete task(s)
----------------

This marks task(s) as completed, returning its metadata
back to the server.

  * Request

    - type = QueryMsg::Transfer
    - location = hostname completing work
    - task = [TaskMsg]

  * **Response**

    - type = QueryMsg::OK

  * **Response**

    - type = QueryMsg::NotFound - server can't find one or more tasks
    - task = [TaskMsg] - list of tasks not found

worker exit notification
------------------------

This marks the worker as down.  All its assigned
tasks are moved back to the `ready` pool.

  * Request

    - type = QueryMsg::Exit
    - location = worker hostname to mark down

  * **Response**

    - type = QueryMsg::OK

  * **Response**

    - type = QueryMsg::NotFound - FYI - the worker was not listed as active

