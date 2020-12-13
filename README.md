# dwork

Task graph scheduler with a minimalistic API.

# installing

dwork builds using cmake.  An example build.sh script is included.
It requires the following dependencies:

* [TKRZW](https://dbmx.net/tkrzw)
* [Protobuf](https://developers.google.com/protocol-buffers/docs/cpptutorial)
* [hiredis](https://github.com/redis/hiredis) and a running redis server
   - presently the code is hardwired to use localhost on the default port
* [cppzmq](https://github.com/zeromq/cppzmq)
   - more information on ZeroMQ at https://zeromq.org/

# usage

* Most people will be interested in the following workflow:
  1) run the dwork hub `bin/dhub`
  2) load up a list of tasks `bin/dadd`
  3) start up a pool of workers `see example in bin/worker` (can also use bin/query from cmdline)
  4) monitor task progress `bin/dquery`

Steps 1, 2 and 3 can be run in any order as long as
2 comes after 1.

# task routing

Full task references are (<TaskID>, <Worker>).  When a reference
is encountered during processing, it triggers a `retrieve()`,
`add_successor()` or a `delete()` message.
The first two use the last-known task owner, which is updated
if incorrect (lookup caching).
Delete messages traverse the entire task
ownership chain, and need to use the `first-known` task owner link.

# Building the Documentation

The following packages are required to build the docs:

* cmake
* doxygen
* python3-sphinx

In addition, you'll need some python packages:
```
pip3 install sphinx_rtd_theme
pip3 install breathe
```
