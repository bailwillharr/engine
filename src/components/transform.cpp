#include "components/transform.hpp"

#include <iostream>

namespace components {

Transform::Transform(Object* parent) : Component(parent, TypeEnum::TRANSFORM)
{

}

Transform::~Transform()
{
}

}
