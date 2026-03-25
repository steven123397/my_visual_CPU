#pragma once

class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;

    virtual void step() = 0;
    virtual const char* name() const = 0;
};
