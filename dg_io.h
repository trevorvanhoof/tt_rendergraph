#pragma once

#include "dg.h"

/*
Example json:

{
"nodes": [{
"type": "MulF32",
"label": "",
"inputs": [{
"type": "F32",
"label": "a",
"value": 2.0
}, {
"type": "F32",
"label": "b",
"value": 3.0
}],
"outputs": [{
"type": "F32",
"label": "result"
}]
}],
"connections": [{ 
"source": {
"nodeId": 0,
"socketLabel": "a",
"socketArrayIndices": []
},
"destination": {
"nodeId": 0,
"socketLabel": "b",
"socketArrayIndices": []
} 
}]
}
*/

class GraphSerializer {
private:
    struct SocketPath {
        size_t nodeId;
        std::string socketLabel;
        std::vector<size_t> socketArrayIndices;
    };

    std::unordered_map<const ISocket*, SocketPath> serializedSockets;
    std::vector<std::pair<const ISocket*, const ISocket*>> connectionQueue;

    void serializeInputs(const ISocket& socket, const SocketPath& path);
    TTJson::Object serialize(const SocketPath& path);
    TTJson::Object serialize(const ISocket& socket, const SocketPath& path);
    TTJson::Object serialize(const Node& node, size_t nodeId);

    ISocket* findSocket(const std::vector<ISocket*>& sockets, const std::string& label, const std::vector<size_t>& indices);
    ISocket* findSocket(const Node& node, const std::string& label, const std::vector<size_t>& indices);
    void deserializeSocket(const TTJson::Object& socketObj, Node& node, bool isOutput);
    Node* deserializeNode(const TTJson::Object& nodeObj);
    ISocket* deserializeSocketPath(const TTJson::Object& connectionObj, const std::vector<Node*>& nodes);

public:
    TTJson::Object serialize(const std::vector<Node*>& nodes);

    typedef std::function<Node& (const std::string& label)> nodeCreatorFn;
    typedef std::function<ISocket* (const std::string& label, bool isOutput, Node& node)> socketCreatorFn;

    // TODO: how are we going to fill this without a ton of manual bookkeeping?
    std::unordered_map<std::string, nodeCreatorFn> nodeFactory;
    std::unordered_map<std::string, socketCreatorFn> socketFactory;

    std::vector<Node*> deserializeGraph(const TTJson::Value& document);

    std::vector<std::string> deserializeErrors;
};
