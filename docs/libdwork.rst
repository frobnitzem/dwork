libdwork - The Library
######################

Workers can be implemented in lots of different ways.
You might want to build your own client
to interact with `dhub`.

You can do this on your own following the documented
message API and build based on ZeroMQ and protocol buffers.
*Or*, you can write a C++ program that uses our
convenient wrapper classes and functions.


.. doxygenfile:: proto.hh

.. doxygenclass:: dwork::TaskDB
   :members:

.. doxygenclass:: dwork::TaskHeap
   :members:

