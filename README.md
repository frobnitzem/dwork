# dwork

Task graph scheduler with a minimalistic API.

# installing

dwork builds using cmake.  An example build.sh script is included.
It requires the following dependencies:

* [TKRZW](https://dbmx.net/tkrzw)
* [Protobuf](https://developers.google.com/protocol-buffers/docs/cpptutorial)
* [cppzmq](https://github.com/zeromq/cppzmq)
   - more information on ZeroMQ at https://zeromq.org/

# Basic Usage

Most people will be interested in the following workflow:

  1) run the dwork hub `dhub &`
  2) load up a list of tasks `dquery addfile <file listing 1 task per line>`
  3) start up a pool of workers `see example in tests/kworker` (can also use `dquery steal/complete` from shell)
    - Note: Processor elements that steal a task are responsible for calling `complete` when it's finished,
      or else the task's successors will not run.
  4) monitor task progress `bin/dquery status`

Steps 1, 2 and 3 can be run in any order as long as
2 comes after 1.

What's happening in the background is that `dhub` is listening for client connections on tcp port 6125.
The `dquery` tool sends and receives messages from the server to accomplish adding/dequeuing tasks.
All valid messages are enumerated documentation.
The underlying format is in [the protobuf file](https://github.com/frobnitzem/dwork/blob/master/proto/TaskMsg.proto).

# Advanced Usage

The behavior of the above programs can be altered by command-line arguments.  Notably, `dhub` accepts a "-f"
argument to store/recover its task database to/from file.  `dquery` also accepts several options that
are documented by running `dquery -h`.

It is our intent to build variants of `dquery` that work as (library-level) function calls from various
languages (python being a major one).  Basically, this just requires
creating machinery to send/receive protobuf over zmq in those languages.
If you are interested in this project, contact the developers directly!

A C++ variant is essentially done, since it just requires adapting `test/kworker` and linking to `libdwork`.
The bash variant is `dquery` itself.

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
