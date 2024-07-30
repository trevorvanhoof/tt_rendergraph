# tt_rendergraph

An implementation of a dependency graph with templated (so strongly typed) sockets/plugs/ports, avoiding the need for optional/dynamic_cast/rtti shenanigans.

Made over a weekend as a proof of concept for a friend of mine, on top of whatever the version of my other utility libraries was at that time.
So it relies on these, but I failed to save which commit hash and things likely won't compile or run at this point:
https://github.com/trevorvanhoof/tt_cpplib
https://github.com/trevorvanhoof/tt_rendering

---

The core concept is still valid though!

We have a node which holds sockets.
A socket holds its value, and an optional input connection which is a socket of the same type.

Whenever a connection is made or broken, or the socket's value is set, the graph propagates this changed state (called "dirty") in a "push" mode.
In this mode, we take the node that the socket belongs to, and flag it so that it will recompute its outputs when necessary.
For each output, if it is connected to another socket's "input", we tell that node that it's input has changed by proxy; for as soon as we recompute, it will have.
This recurses the graph until all down-stream nodes are flagged "dirty".

Whenever an output's value is requested we operate in a "pull" mode. The socket will check if the owning node is "dirty" and call compute on it if necessary, before returning the stored value.
In this compute method, input socket values are (probably) requested, and if they have an incoming connection, that socket's value is requested instead.
That recursively triggers pulling outputs on other nodes and computing them before continuing the computation on the original node.

As a failsafe, nodes are set "clean" before they are computed, so that a cycle in the graph results in unpredictable behaviour (may or may not yield out-of-date socket values) instead of an infinite recursion.

As a fun extra challenge we also have the concept of array sockets, these do not own inputs or values directly, but are just a list of sockets.
It does require our socket base type to implement an isArray boolean which is the only part of the code that is not statically typed and requires some runtime checks.

---

To extend the graph, we implement templated socket types, as demonstrated in https://github.com/trevorvanhoof/tt_rendergraph/blob/main/rendering_nodes.h, where really only the serialize/deserialize implementation is necessary. Then we define nodes that have socket members and a compute method that can read from and write to its own sockets.
