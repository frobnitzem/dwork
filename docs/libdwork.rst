libdwork - The Library
######################

Workers can be implemented in lots of different ways.
You might want to build your own client
to interact with `dhub`.

You can do this on your own following the documented
message API based on ZeroMQ and protocol buffers.
*Or*, you can write a C++ program that uses our
convenient wrapper classes and functions.


An example of using the protobuf interface is below::

    int send_task(zmq::socket_t &hub) {
        dwork::QueryMsg query;
        query.set_type(dwork::QueryMsg::Create);
        dwork::TaskMsg *task = new_task(query, "cook dinner");

        dwork::TaskMsg::Dep *dep1 = task->add_pred();
        dep1->set_name("chop onions");
        dwork::TaskMsg::Dep *dep2 = task->add_pred();
        dep2->set_name("gather spices");

        sendProto(hub, query);
        auto resp = recvProto<dwork::QueryMsg>(hub);
        return !(resp.has_value() && resp.value().type() == dwork::QueryMsg::OK);
    }

Documentation on the methods available to manipulate the dwork::TaskMsg
and dwork::QueryMsg classes are available here:
https://developers.google.com/protocol-buffers/docs/reference/cpp-generated

Our send/receive wrappers are in `proto.hh`:

.. doxygenfile:: proto.hh

.. doxygenclass:: dwork::TaskDB
   :members:

.. doxygenclass:: dwork::TaskHeap
   :members:

