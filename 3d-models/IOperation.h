#pragma once
#include "Polyhedron.h"
class IOperation {
public:
    virtual ~IOperation() = default;

    virtual const char* name() const = 0;

    // Панель параметров операции (угол поворота, коэффициент масштаба...).
    virtual void drawParamsUI() {}

    // Применить к объекту. Вызывается по кнопке "Применить".
    virtual void apply(Polyhedron& obj) = 0;
};