#include "Simple_Point.hh"

#include "Material.hh"
#include "XML_Node.hh"

using std::shared_ptr;
using std::vector;

Simple_Point::
Simple_Point(int index,
             int dimension,
             Point_Type point_type,
             shared_ptr<Material> material,
             vector<double> const &position):
    index_(index),
    dimension_(dimension),
    point_type_(point_type),
    material_(material),
    position_(position)
{
}

void Simple_Point::
output(XML_Node output_node) const
{
    XML_Node point_node = output_node.append_child("point");
    point_node.set_attribute(index_, "index");
    point_node.set_child_value(dimension_, "dimension");
    point_node.set_child_vector(position_, "position");
    point_node.set_child_value(material_->index(), "material_index");
}

void Simple_Point::
check_class_invariants() const
{
    Assert(material_);
    Assert(position_.size() == dimension_);
}
                        
