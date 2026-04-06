#pragma once
#include "geom.h"
#include <boost/serialization/vector.hpp>

namespace boost {
namespace serialization {

template<class Archive>
void serialize(Archive& ar, geom::Point2D& point, [[maybe_unused]] const unsigned int version) {
    ar& point.x;
    ar& point.y;
}

}
}