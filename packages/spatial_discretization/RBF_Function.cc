#include "RBF_Function.hh"

#include "Distance.hh"
#include "RBF.hh"
#include "XML_Functions.hh"

RBF_Function::
RBF_Function(double shape,
             vector<double> const &position,
             shared_ptr<RBF> rbf,
             shared_ptr<Distance> distance):
    shape_(shape),
    position_(position),
    rbf_(rbf),
    distance_(distance)
{
}

int RBF_Function::
dimension() const
{
    return distance_->dimension();
}

double RBF_Function::
radius() const
{
    switch(rbf_->range())
    {
    case RBF::Range::LOCAL:
        return rbf_->radius() / shape;
    case RBF::Range::GLOBAL:
        return numeric_limits<double>::max();
    }
}

double RBF_Function::
basis(vector<double> const &r) const
{
    double dist = distance_->distance(r,
                                      position_);
    return rbf_->basis(shape_ * dist);
}

double RBF_Function::
d_basis(int dim,
        vector<double> const &r) const
{
    double dist = distance_->distance(r,
                                      position_);
    double d_dist = distance_->d_distance(dim,
                                          r,
                                          position_);
    
    return rbf_->d_basis(shape_ * dist,
                         shape_ * d_dist);
}


double RBF_Function::
dd_basis(int dim,
         vector<double> const &r) const
{
    double dist = distance_->distance(r,
                                      position_);
    double d_dist = distance_->d_distance(dim,
                                          r,
                                          position_);
    double dd_dist = distance_->dd_distance(dim,
                                            r,
                                            position_);

    return rbf_->dd_basis(shape_ * dist,
                          shape_ * shape_ * d_dist * d_dist,
                          shape_ * dd_dist);
}

vector<double> RBF_Function::
gradient_basis(vector<double> const &r) const               
{
    double dist = distance_->distance(group,
                                      r,
                                      position_);
    vector<double> grad = distance_->gradient_distance(r,
                                                       position_);

    int dimension = distance_->dimension();
    
    vector<double> res(dimension);
    
    for (int d = 0; d < dimension; ++d)
    {
        res[d] = rbf_->d_basis(shape_ * dist,
                               shape_ * grad[d]);
    }

    return res;
}

double RBF_Function::
laplacian(vector<double> const &r)
{
    double dist = distance_->distance(r,
                                      position_);
    vector<double> grad = distance_->gradient_distance(r,
                                                       position_);
    double lap = distance_->laplacian_distance(r,
                                               position_);

    int dimension = distance_->dimension();

    double grad2 = 0;
    for (int d = 0; d < dimension; ++d)
    {
        grad2 += grad[d] * grad[d];
    }
    
    return rbf_->dd_basis(shape_ * dist,
                          shape_ * shape_ * grad2 * grad2,
                          shape_ * lap);
}

void RBF_Function::
output(XML_Node output_node) const
{
    XML_Node rbf_node = output_node.append_child("rbf_function");

    rbf_node.set_child_value(shape_, "shape");
    rbf_node.set_child_vector(position_, "position");
    rbf_node.set_child_value(rbf_->description(), "rbf_type");
    rbf_node.set_child_value(distance_->description(), "distance_type");
}

void RBF_Function::
check_class_invariants() const
{
    Assert(shape_ > 0);
    Assert(position_.size() == distance_->dimension());
    Assert(rbf_);
    Assert(distance_);
}
