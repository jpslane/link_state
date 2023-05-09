Files
---------
- manager_send:                    executable on manager side,
- lsrouter:                        executable on node side using link state protocol,
- manager_send.cpp:                source of manager_send,
- main.cpp, monitor_neighbors.hpp: source of lsrouter,
- make_topology.pl:                make a topology of nodes,
- example_topology folder:         example topologies

Design
-------
Manager makes a topology of nodes and gives path cost to each node. Each node runs a link state protocol for its routing table. Most of the implementation is contained in monitor_neighbors.hpp.
