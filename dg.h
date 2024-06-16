#pragma once

#include <string>
#include <vector>
#include <set>
#include <assert.h>

#include "../tt_cpplib/tt_json5.h"

class Node;

class ISocket {
private:
    std::string _label;
    bool _isOutput;
    Node& _node;

    friend class GraphSerializer;
    friend class ISocketArray;
    virtual bool isArray() const = 0; 

protected:
    virtual bool deserializeValue(const TTJson::Value& value) { return false; }
    virtual TTJson::Value serializeValue() const { return TTJson::Value(); }
    virtual ISocket* _getInput() const { return nullptr; }
    virtual void _setInput(ISocket& input) {};
    virtual std::string typeName() const = 0;

    void _dirtyNode() const;
    void _computeNode() const;

public:
    ISocket(const std::string& label, bool isOutput, Node& node);
    const std::string& label() const { return _label; }
    bool isOutput() const { return _isOutput; }
    Node& node() const { return _node; }
    virtual std::set<ISocket*> outputs() const = 0;
    std::string debugStr() const; // TODO: deprecated
};

template<typename T, typename CRTP, const char* NAME> class Socket : public ISocket {
private:
    Socket<T, CRTP, NAME>* _input = nullptr;
    std::set<ISocket*> _outputs {};
    ISocket* _getInput() const override { return _input; };
    void _setInput(ISocket& input) override { setInput(*(CRTP*)&input); }

protected:
    T _value;

public:
    typedef T value_t;

    Socket(const std::string& label, const T& initialValue, bool isOutput, Node& node) 
        : _value(initialValue), ISocket(label, isOutput, node) {}

    T& value() {
        if(_input)
            return _input->value();
        if (isOutput())
            _computeNode();
        return _value; 
    }
    const CRTP* input() const { return _input; }
    std::set<ISocket*> outputs() const override { return _outputs; }

    void setValue(const T& value) { 
        _value = value; 
        if (!_input)
            _dirtyNode();
    }

    void setInput(CRTP& input) {
        if (_input == (Socket<T, CRTP, NAME>*)&input) return; 
        if (_input) _input->_outputs.erase(this);
        _input = (Socket<T, CRTP, NAME>*)&input; 
        _input->_outputs.insert(this);
        _dirtyNode();
    }

    void disconnect() {
        if (!_input) return; 
        if (_input) _input->_outputs.erase(this);
        _input = nullptr;
        _dirtyNode();
    }

    bool isArray() const override { return false; }
    std::string typeName() const { return NAME; } // typeid(T).name(); }
};

class ISocketArray : public ISocket {
    using ISocket::ISocket;

protected:
    friend class GraphSerializer;
    std::vector<ISocket*> _children {};
    virtual ISocket* _appendNew() = 0;
    bool deserializeValue(const TTJson::Value& value) override;
    TTJson::Value serializeValue() const override;
    // TODO: We do not have dynamic array support because we can't generate socket factory functions for SocketArray<> instantiations.
    std::string typeName() const override { return "<array>"; }
};

template<typename SocketT> class SocketArray : public ISocketArray {
private:
    typename SocketT::value_t _defaultValue;
    ISocket* _appendNew() override { return &appendNew(); }

public:
    SocketArray(const std::string& label, const typename SocketT::value_t& defaultValue, bool isOutput, Node& node) 
        : _defaultValue(defaultValue), ISocketArray(label, isOutput, node) {}

    const std::vector<SocketT*>& children() const { return *(const std::vector<SocketT*>*)&_children; }

    std::set<ISocket*> outputs() const override { 
        std::set<ISocket*> outputs;
        for (const auto& child : _children) {
            const auto& childInputs = child->outputs();
            outputs.insert(childInputs.begin(), childInputs.end());
        }
        return outputs;
    }

    SocketT& appendNew() {
        std::string subLabel = label() + "[" + std::to_string(_children.size()) + "]";
        SocketT* socket = new SocketT(subLabel, _defaultValue, isOutput(), node());
        _children.push_back(socket);
        return *socket;
    }

    bool isArray() const override { return true; }
};

class Node {
private:
    friend class GraphSerializer;
    std::string _label;
    std::vector<ISocket*> _inputs {};
    std::vector<ISocket*> _outputs {};
    bool _dirty = true;
    bool _computing = false;

    // TODO: this is only used by the serializer, can we do something else?
    virtual std::string typeName() const = 0;

protected:
    bool _initializing = true;
    virtual void _compute() {}
    virtual void _socketChanged(const ISocket& socket) {}

public:
    Node(const std::string& label = "");
    const std::string& label() const { return _label; }
    std::string debugStr() const { return "\"" + typeName() + "\":" + _label; }

    template<typename SocketT> SocketT& addInput(const std::string& label, const typename SocketT::value_t& initialValue) {
        SocketT* socket = new SocketT(label, initialValue, false, *this);
        _inputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    template<typename SocketT> SocketArray<SocketT>& addArrayInput(const std::string& label, const typename SocketT::value_t& initialValue) {
        SocketArray<SocketT>* socket = new SocketArray<SocketT>(label, initialValue, false, *this);
        _inputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    template<typename T> T& addOutput(const std::string& label, const typename T::value_t& initialValue) {
        T* socket = new T(label, initialValue, true, *this);
        _outputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    template<typename SocketT> SocketT& addArrayOutput(const std::string& label, const typename SocketT::value_t& initialValue) {
        SocketArray<SocketT>* socket = new SocketArray<SocketT>(label, initialValue, true, *this);
        _outputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    void compute();
    void dirty(const ISocket& changed);
};

namespace {
    // json utils that I may move to the core library later
    const TTJson::Value* objTryGet(const TTJson::Object& obj, const TTJson::str_t& key) {
        auto it = obj.find(key);
        if (it != obj.end()) return &it->second;
        return nullptr;
    }

    // TODO: Allow cast from int / double?
    const bool* objTryGetBool(const TTJson::Object& obj, const TTJson::str_t& key) {
        const TTJson::Value* value = objTryGet(obj, key);
        if (value && value->isBool()) return &value->asBool();
        return nullptr;
    }

    // TODO: Allow cast from double / bool?
    const long long* objTryGetInt(const TTJson::Object& obj, const TTJson::str_t& key) {
        const TTJson::Value* value = objTryGet(obj, key);
        if (value && value->isInt()) return &value->asInt();
        return nullptr;
    }

    // TODO: Allow cast from int / bool?
    const double* objTryGetDouble(const TTJson::Object& obj, const TTJson::str_t& key) {
        const TTJson::Value* value = objTryGet(obj, key);
        if (value && value->isDouble()) return &value->asDouble();
        return nullptr;
    }

    const TTJson::str_t* objTryGetString(const TTJson::Object& obj, const TTJson::str_t& key) {
        const TTJson::Value* value = objTryGet(obj, key);
        if (value && value->isString()) return &value->asString();
        return nullptr;
    }

    const TTJson::Array* objTryGetArray(const TTJson::Object& obj, const TTJson::str_t& key) {
        const TTJson::Value* value = objTryGet(obj, key);
        if (value && value->isArray()) return &value->asArray();
        return nullptr;
    }

    const TTJson::Object* objTryGetObject(const TTJson::Object& obj, const TTJson::str_t& key) {
        const TTJson::Value* value = objTryGet(obj, key);
        if (value && value->isObject()) return &value->asObject();
        return nullptr;
    }
}

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
    /// Serialization
    struct SocketPath {
        size_t nodeId;
        std::string socketLabel;
        std::vector<size_t> socketArrayIndices;
    };
    std::unordered_map<const ISocket*, SocketPath> serializedSockets;
    std::vector<std::pair<const ISocket*, const ISocket*>> connectionQueue;

public:
    TTJson::Object serialize(const std::vector<Node*>& nodes) {
        TTJson::Object result;

        TTJson::Array outNodes;
        size_t nodeId = 0;
        for (const auto& node : nodes)
            outNodes.push_back(serialize(*node, nodeId++));
        result["nodes"] = outNodes;

        TTJson::Array outConnections;
        for(const auto& pair : connectionQueue) {
            // TODO: assert instead and fix why this is happening
            if (pair.first == nullptr || pair.second == nullptr) continue;

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

        return result;
    }

private:
    TTJson::Object serialize(const SocketPath& path) {
        TTJson::Object result;
        result["nodeId"] = (long long)path.nodeId;
        result["socketLabel"] = (TTJson::str_t)path.socketLabel;
        TTJson::Array indices;
        for(size_t i : path.socketArrayIndices)
            indices.push_back((long long)i);
        result["socketArrayIndices"] = indices;
        return result;
    }

    TTJson::Object serialize(const Node& node, size_t nodeId) {
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

    void serializeInputs(const ISocket& socket, const SocketPath& path) {
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

    TTJson::Object serialize(const ISocket& socket, const SocketPath& path) {
        serializeInputs(socket, path);

        TTJson::Object socketObj;
        socketObj["type"] = (TTJson::str_t)socket.typeName();
        socketObj["label"] = socket.label();
        socketObj["value"] = socket.serializeValue();
        return socketObj;
    }

    /// Deserialization
public:
    typedef std::function<Node&(const std::string& label)> nodeCreatorFn;
    typedef std::function<ISocket*(const std::string& label, bool isOutput, Node& node)> socketCreatorFn;
    // typedef Node* (*nodeCreatorFn) (const std::string& label);
    // typedef ISocket* (*socketCreatorFn) (const std::string& label, bool isOutput, Node& node);

    // TODO: how are we going to fill this without a ton of manual bookkeeping?
    std::unordered_map<std::string, nodeCreatorFn> nodeFactory;
    std::unordered_map<std::string, socketCreatorFn> socketFactory;

private:
    ISocket* findSocket(const std::vector<ISocket*>& sockets, const std::string& label, const std::vector<size_t>& indices) {
        // Note: this assumes socket names are unique.
        for (ISocket* socket : sockets) {
            if (socket->label() == label) {
                for (size_t i : indices) {
                    if (!socket->isArray()) 
                        return nullptr; // malformed json
                    socket = ((ISocketArray*)socket)->_children[i];
                }
                return socket;
            }
        }
        return nullptr;
    }

    ISocket* findSocket(const Node& node, const std::string& label, const std::vector<size_t>& indices) {
        ISocket* result = findSocket(node._inputs, label, indices);
        if (result)
            return result;
        return findSocket(node._outputs, label, indices);
    }

    // only returns a value if it is a valid socket not already found on the node
    void deserializeSocket(const TTJson::Object& socketObj, Node& node, bool isOutput) {
        auto type = objTryGetString(socketObj, "type");
        auto label = objTryGetString(socketObj, "label");
        
        if(!label || !type) return; // malformed json
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
            if (it == socketFactory.end()) return; // unknown socket type
            into = it->second(*label, isOutput, node);
            (isOutput ? node._outputs : node._inputs).push_back(into);
        }

        auto value = objTryGet(socketObj, "value");
        if(value)
            into->deserializeValue(*value);
    }

    Node* deserializeNode(const TTJson::Object& nodeObj) {
        // get type to spawn
        auto type = objTryGetString(nodeObj, "type");
        if(!type) return nullptr; // malformed json
        const auto& it2 = nodeFactory.find(*type);
        if (it2 == nodeFactory.end()) return nullptr; // unknown node type

        // spawn
        auto label = objTryGetString(nodeObj, "label");
        Node& instance = it2->second(label ? *label : "");

        // fill
        auto inputs = objTryGetArray(nodeObj, "inputs");
        if(inputs) {
            for(const auto& input : *inputs) {
                if (!input.isObject()) continue; // malformed json
                deserializeSocket(input.asObject(), instance, false);
            }
        }

        auto outputs = objTryGetArray(nodeObj, "outputs");
        if(outputs) {
            for(const auto& output : *outputs) {
                if (!output.isObject()) continue; // malformed json
                deserializeSocket(output.asObject(), instance, true);
            }
        }

        return &instance;
    }

    ISocket* deserializeSocketPath(const TTJson::Object& connectionObj, const std::vector<Node*>& nodes) {
        auto nodeId = objTryGetInt(connectionObj, "nodeId");
        auto socketLabel = objTryGetString(connectionObj, "socketLabel");
        auto socketArrayIndices = objTryGetArray(connectionObj, "socketArrayIndices");
        if (!nodeId || !socketLabel) return nullptr; // malformed json

        std::vector<size_t> arrayIndices;
        if(socketArrayIndices) {
            for(const auto& socketArrayIndex : *socketArrayIndices) {
                if (!socketArrayIndex.isInt()) continue; // malformed json
                arrayIndices.push_back(socketArrayIndex.asInt());
            }
        }

        return findSocket(*nodes[*nodeId], *socketLabel, arrayIndices);
    }

public:
    std::vector<Node*> deserializeGraph(const TTJson::Value& document) {
        if (!document.isObject()) return {}; // malformed json

        std::vector<Node*> result;
        auto nodeObjs = objTryGetArray(document.asObject(), "nodes");
        if(nodeObjs) {
            for(const auto& nodeObj : *nodeObjs) {
                if (!nodeObj.isObject()) continue; // malformed json
                Node* node = deserializeNode(nodeObj.asObject());
                if (!node) continue; // malformed json or missing factory method
                result.push_back(node);
            }
        }

        auto connectionObjs = objTryGetArray(document.asObject(), "connections");
        if(connectionObjs) {
            for(const auto& connectionObj : *connectionObjs) {
                if (!connectionObj.isObject()) continue; // malformed json
                auto sourceObj = objTryGetObject(connectionObj.asObject(), "source");
                auto destinationObj = objTryGetObject(connectionObj.asObject(), "destination");
                if (!sourceObj || !destinationObj) continue; // malformed json
                ISocket* source = deserializeSocketPath(*sourceObj, result);
                ISocket* destination = deserializeSocketPath(*destinationObj, result);
                destination->_setInput(*source);
            }
        }
        return result;
    }
};
