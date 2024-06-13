#include "dg.h"

ISocket::ISocket(const std::string& label, bool isOutput, Node& node) 
    : _label(label), _isOutput(isOutput), _node(node) {}

void ISocket::_dirtyNode() const { 
    _node.dirty(*this); 
}

void ISocket::_computeNode() const { 
    _node.compute(); 
}

std::string ISocket::debugStr() const {
    return _node.debugStr() + "." + _label;
}


Node::Node(const std::string& label) 
    : _label(label) {}

void Node::compute() {
    assert(!_initializing);
    if (!_dirty) return;
    _dirty = false;
    _computing = true;
    _compute();
    _computing = false;
}

void Node::dirty(const ISocket& changed) {
    // Make sure we are not writing to the wrong type of socket from the wrong place.
    assert(!_initializing);
    if (_computing) {
        assert(changed.isOutput());
        return;
    }
    assert(!changed.isOutput());

    // We can watch for specific socket changes to e.g. (re-)generate sockets based on input values.
    _socketChanged(changed);
    if (_dirty)
        return;

    _dirty = true;

    // Dirty dependents
    for(const auto& output : _outputs)
        for(auto& other : output->outputs())
            other->node().dirty(*other);
}

