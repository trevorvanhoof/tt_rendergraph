#pragma once

#include <string>
#include <vector>
#include <set>

#include "../tt_cpplib/tt_json5.h"
#include "../tt_cpplib/tt_messages.h"

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

    ISocket(const ISocket& rhs) = delete;
    ISocket(ISocket&& rhs) = delete;
    ISocket& operator=(const ISocket& rhs) = delete;
    ISocket& operator=(ISocket&& rhs) = delete;
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
    static std::string sTypeName() { return NAME; }
    std::string typeName() const override { return NAME; }
};

class ISocketArray : public ISocket {
    using ISocket::ISocket;

protected:
    friend class GraphSerializer;
    std::vector<ISocket*> _children {};
    virtual ISocket* _appendNew() = 0;
    bool deserializeValue(const TTJson::Value& value) override;
    TTJson::Value serializeValue() const override;
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
    static std::string sTypeName() { return "SocketArray<" + SocketT::sTypeName() + ">"; }
    std::string typeName() const override { return "SocketArray<" + SocketT::sTypeName() + ">"; }
};

class Node {
private:
    friend class GraphSerializer;
    std::string _label;
    std::vector<ISocket*> _inputs {};
    std::vector<ISocket*> _outputs {};
    bool _dirty = true;
    bool _computing = false;

    virtual std::string typeName() const = 0;

protected:
    bool _initializing = true;
    virtual void _compute() {}
    virtual void _socketChanged(const ISocket& socket) {}

public:
    Node(const std::string& label = "");
    const std::string& label() const { return _label; }

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
