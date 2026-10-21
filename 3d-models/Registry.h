#pragma once
#include <memory>
#include <vector>
#include "IGeometrySource.h"
#include "IOperation.h"
class Registry {
public:
    void addSource(std::unique_ptr<IGeometrySource> s) {
        sources_.push_back(std::move(s));
    }
    void addOperation(std::unique_ptr<IOperation> op) {
        operations_.push_back(std::move(op));
    }

    const std::vector<std::unique_ptr<IGeometrySource>>& sources() const { return sources_; }
    const std::vector<std::unique_ptr<IOperation>>& operations() const { return operations_; }

private:
    std::vector<std::unique_ptr<IGeometrySource>> sources_;
    std::vector<std::unique_ptr<IOperation>>      operations_;
};