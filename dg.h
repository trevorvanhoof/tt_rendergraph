#pragma once

#include <string>
#include <vector>
#include <set>
#include <assert.h>

class Node;

class ISocket {
private:
    std::string _label;
    bool _isOutput;
    Node& _node;

protected:
    void _dirtyNode() const;
    void _computeNode() const;

public:
    ISocket(const std::string& label, bool isOutput, Node& node);
    const std::string& label() const { return _label; }
    bool isOutput() const { return _isOutput; }
    Node& node() const { return _node; }
    virtual std::set<ISocket*> outputs() const = 0;
    std::string debugStr() const;
};

template<typename T> class Socket : public ISocket {
private:
    T _value;
    Socket<T>* _input = nullptr;
    std::set<ISocket*> _outputs {};

public:
    Socket(const std::string& label, const T& initialValue, bool isOutput, Node& node) 
        : _value(initialValue), ISocket(label, isOutput, node) {}

    T& value() {
        if(_input)
            return _input->value();
        if (isOutput())
            _computeNode();
        return _value; 
    }
    const Socket<T>* input() const { return _input; }
    std::set<ISocket*> outputs() const override { return _outputs; }

    void setValue(const T& value) { 
        _value = value; 
        if (!_input)
            _dirtyNode();
    }

    void setInput(Socket<T>& input) {
        if (_input == &input) return; 
        if (_input) _input->_outputs.erase(this);
        _input = &input; 
        input._outputs.insert(this);
        _dirtyNode();
    }

    void disconnect() {
        if (!_input) return; 
        if (_input) _input->_outputs.erase(this);
        _input = nullptr;
        _dirtyNode();
    }
};

template<typename T> class SocketArray : public ISocket {
private:
    T _defaultValue;
    std::vector<Socket<T>*> _children {};

public:
    SocketArray(const std::string& label, const T& defaultValue, bool isOutput, Node& node) 
        : _defaultValue(defaultValue), ISocket(label, isOutput, node) {}

    const std::vector<Socket<T>*>& children() const { return _children; }

    std::set<ISocket*> outputs() const override { 
        std::set<ISocket*> outputs;
        for (const auto& child : _children) {
            const auto& childInputs = child->outputs();
            outputs.insert(childInputs.begin(), childInputs.end());
        }
        return outputs;
    }

    Socket<T>& appendNew() {
        std::string subLabel = label() + "[" + std::to_string(_children.size()) + "]";
        Socket<T>* socket = new Socket<T>(subLabel, _defaultValue, isOutput(), node());
        _children.push_back(socket);
        return *socket;
    }
};

class Node {
private:
    std::string _label;
    std::vector<ISocket*> _inputs {};
    std::vector<ISocket*> _outputs {};
    bool _dirty = true;
    bool _computing = false;

protected:
    bool _initializing = true;
    virtual void _compute() {}
    virtual void _socketChanged(const ISocket& socket) {}

public:
    Node(const std::string& label = "");
    const std::string& label() const { return _label; }
    virtual std::string debugStr() const { return "\"Node\":" + _label; }

    template<typename T> Socket<T>& addInput(const std::string& label, const T& initialValue) {
        Socket<T>* socket = new Socket<T>(label, initialValue, false, *this);
        _inputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    template<typename T> SocketArray<T>& addArrayInput(const std::string& label, const T& initialValue) {
        SocketArray<T>* socket = new SocketArray<T>(label, initialValue, false, *this);
        _inputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    template<typename T> Socket<T>& addOutput(const std::string& label, const T& initialValue) {
        Socket<T>* socket = new Socket<T>(label, initialValue, true, *this);
        _outputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    template<typename T> Socket<T>& addArrayOutput(const std::string& label, const T& initialValue) {
        SocketArray<T>* socket = new SocketArray<T>(label, initialValue, true, *this);
        _outputs.push_back(socket);
        if (!_initializing)
            dirty(*socket);
        return *socket;
    }

    void compute();
    void dirty(const ISocket& changed);
};
