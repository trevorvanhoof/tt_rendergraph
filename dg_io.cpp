#include "dg_io.h"

/// Serialization
TTJson::Object GraphSerializer::serialize(const std::vector<Node*>& nodes) {
    TTJson::Object result;

    TTJson::Array outNodes;
    size_t nodeId = 0;
    for (const auto& node : nodes)
        outNodes.push_back(serialize(*node, nodeId++));
    result["nodes"] = outNodes;

    TTJson::Array outConnections;
    for(const auto& pair : connectionQueue) {
        TT::assert(pair.first != nullptr && pair.second != nullptr);

        auto it1 = serializedSockets.find(pair.first);
        auto it2 = serializedSockets.find(pair.second);
        if (it1 == serializedSockets.end() || it2 == serializedSockets.end())
            continue;

        TTJson::Object outConnection;
        outConnection["source"] = serialize(it1->second);
        outConnection["destination"] = serialize(it2->second);
        outConnections.push_back(outConnection);
    }
    result["connections"] = outConnections;

    // Cleanup
    serializedSockets.clear();
    connectionQueue.clear();

    return result;
}

TTJson::Object GraphSerializer::serialize(const SocketPath& path) {
    TTJson::Object result;
    result["nodeId"] = (long long)path.nodeId;
    result["socketLabel"] = (TTJson::str_t)path.socketLabel;
    TTJson::Array indices;
    for(size_t i : path.socketArrayIndices)
        indices.push_back((long long)i);
    result["socketArrayIndices"] = indices;
    return result;
}

TTJson::Object GraphSerializer::serialize(const Node& node, size_t nodeId) {
    TTJson::Object nodeObj;
    nodeObj["type"] = node.typeName();
    nodeObj["label"] = node.label();

    TTJson::Array inputsArray;
    for(const ISocket* socket : node._inputs) {
        SocketPath path { nodeId, socket->label(), {} };
        inputsArray.push_back(serialize(*socket, path));
    }
    nodeObj["inputs"] = inputsArray;

    TTJson::Array outputsArray;
    for(const ISocket* socket : node._outputs) {
        SocketPath path { nodeId, socket->label(), {} };
        outputsArray.push_back(serialize(*socket, path));
    }
    nodeObj["outputs"] = outputsArray;

    return nodeObj;
}

void GraphSerializer::serializeInputs(const ISocket& socket, const SocketPath& path) {
    serializedSockets[&socket] = path;
    if(socket.isArray()) {
        size_t arrayIndex = 0;
        for (const ISocket* element : ((const ISocketArray&)socket)._children) {
            // Copy the parent path
            SocketPath elementPath = { path.nodeId, path.socketLabel, path.socketArrayIndices };
            // Add the array index
            elementPath.socketArrayIndices.push_back(arrayIndex++);
            // Recurse
            serializeInputs(*element, elementPath);
        }
    } else {
        if(socket._getInput())
            connectionQueue.push_back({ socket._getInput(), &socket });
    }
}

TTJson::Object GraphSerializer::serialize(const ISocket& socket, const SocketPath& path) {
    serializeInputs(socket, path);

    TTJson::Object socketObj;
    socketObj["type"] = (TTJson::str_t)socket.typeName();
    socketObj["label"] = socket.label();
    socketObj["value"] = socket.serializeValue();
    return socketObj;
}

/// Deserialization
ISocket* GraphSerializer::findSocket(const std::vector<ISocket*>& sockets, const std::string& label, const std::vector<size_t>& indices) {
    // Note: this assumes socket names are unique.
    for (ISocket* socket : sockets) {
        if (socket->label() == label) {
            for (size_t i : indices) {
                if (!socket->isArray()) {
                    deserializeErrors.push_back("Document gave array index for connection, but the found socket is not an array. Looking for socket: " + label);
                    return nullptr;
                }
                socket = ((ISocketArray*)socket)->_children[i];
            }
            return socket;
        }
    }
    return nullptr;
}

ISocket* GraphSerializer::findSocket(const Node& node, const std::string& label, const std::vector<size_t>& indices) {
    ISocket* result = findSocket(node._inputs, label, indices);
    if (result)
        return result;
    return findSocket(node._outputs, label, indices);
}

// only returns a value if it is a valid socket not already found on the node
void GraphSerializer::deserializeSocket(const TTJson::Object& socketObj, Node& node, bool isOutput) {
    auto type = socketObj.tryGetString("type");
    auto label = socketObj.tryGetString("label");

    std::string nodeErrStr = node.label() + " (" + node.typeName() + ")";
    if(!label || !type) {
        deserializeErrors.push_back("Document missing type or label entry, or entries are not strings, for socket on node: " + nodeErrStr);
        return;
    }
    ISocket* into = nullptr;

    // Note: this assumes socket names are unique.
    for (ISocket* socket : isOutput ? node._outputs : node._inputs) {
        if (socket->label() == *label) {
            into = socket; 
            break;
        }
    }

    if(!into) {
        const auto& it = socketFactory.find(*type);
        if (it == socketFactory.end()) {
            deserializeErrors.push_back("Document gave unknown type for node socket: " + nodeErrStr + "." + *label + " (" + *type + ")");
            return;
        }
        into = it->second(*label, isOutput, node);
        (isOutput ? node._outputs : node._inputs).push_back(into);
    }

    auto value = socketObj.tryGet("value");
    if(value)
        into->deserializeValue(*value);
}

Node* GraphSerializer::deserializeNode(const TTJson::Object& nodeObj) {
    // get type to spawn
    auto type = nodeObj.tryGetString("type");
    if(!type) {
        deserializeErrors.push_back("Document missing type for nodes entry.");
        return nullptr;
    }
    const auto& it2 = nodeFactory.find(*type);
    if (it2 == nodeFactory.end()) {
        deserializeErrors.push_back("Document gave unknown node type: " + *type);
        return nullptr;
    }

    // spawn
    auto label = nodeObj.tryGetString("label");
    Node& instance = it2->second(label ? *label : "");

    std::string nodeErrStr = instance.label() + " (" + *type + ")";

    // fill
    auto inputs = nodeObj.tryGetArray("inputs");
    if(inputs) {
        for(const auto& input : *inputs) {
            if (!input.isObject()) {
                deserializeErrors.push_back("Node inputs in document must be an array of objects. Found something else for node: " + nodeErrStr);
                continue;
            }
            deserializeSocket(input.asObject(), instance, false);
        }
    }

    auto outputs = nodeObj.tryGetArray("outputs");
    if(outputs) {
        for(const auto& output : *outputs) {
            if (!output.isObject()) {
                deserializeErrors.push_back("Node outputs in document must be an array of objects. Found something else for node: " + nodeErrStr);
                continue;
            }
            deserializeSocket(output.asObject(), instance, true);
        }
    }

    return &instance;
}

ISocket* GraphSerializer::deserializeSocketPath(const TTJson::Object& connectionObj, const std::vector<Node*>& nodes) {
    auto nodeId = connectionObj.tryGetInt("nodeId");
    auto socketLabel = connectionObj.tryGetString("socketLabel");
    auto socketArrayIndices = connectionObj.tryGetArray("socketArrayIndices");
    if (!nodeId || !socketLabel) {
        deserializeErrors.push_back("Document contains invalid connection. Missing nodeId, socketLabel or socketArrayIndices, or nodeId is not an int, or socketLabel is not a string, or socketArrayIndices is not an array.");
        return nullptr;
    }

    std::vector<size_t> arrayIndices;
    if(socketArrayIndices) {
        for(const auto& socketArrayIndex : *socketArrayIndices) {
            if (!socketArrayIndex.isInt()) {
                deserializeErrors.push_back("Document contains invalid connection. socketArrayIndices is not an array of ints for socket: " + *socketLabel);
                continue;
            }
            arrayIndices.push_back(socketArrayIndex.asInt());
        }
    }

    if(*nodeId < 0 || (size_t)*nodeId >= nodes.size()) {
        deserializeErrors.push_back("Document contains invalid connection. nodeId is out of bounds for socket: " + *socketLabel);
        return nullptr;
    }

    if(nodes[*nodeId] == nullptr) {
        deserializeErrors.push_back("Failed to connect to node that could not be read. See previous errors for more info.");
        return nullptr;
    }

    return findSocket(*nodes[*nodeId], *socketLabel, arrayIndices);
}

std::vector<Node*> GraphSerializer::deserializeGraph(const TTJson::Value& document) {
    if (!document.isObject()) {
        deserializeErrors.push_back("Document root must be an object.");
        return {};
    }

    std::vector<Node*> graph;
    auto nodeObjs = document.asObject().tryGetArray("nodes");
    if(nodeObjs) {
        for(const auto& nodeObj : *nodeObjs) {
            if (!nodeObj.isObject()) {
                deserializeErrors.push_back("Document contains invalid nodes entry. Must be an object.");
                graph.push_back(nullptr);
                continue;
            }
            Node* node = deserializeNode(nodeObj.asObject());
            if (!node) {
                graph.push_back(nullptr);
                continue;
            }
            graph.push_back(node);

        }
    }

    auto connectionObjs = document.asObject().tryGetArray("connections");
    if(connectionObjs) {
        for(const auto& connectionObj : *connectionObjs) {
            if (!connectionObj.isObject()) continue; // malformed json
            auto sourceObj = connectionObj.asObject().tryGetObject("source");
            auto destinationObj = connectionObj.asObject().tryGetObject("destination");
            if (!sourceObj || !destinationObj) continue; // malformed json
            ISocket* source = deserializeSocketPath(*sourceObj, graph);
            ISocket* destination = deserializeSocketPath(*destinationObj, graph);
            destination->_setInput(*source);
        }
    }
    return graph;
}
