#ifndef Simple_Spatial_Discretization_hh
#define Simple_Spatial_Discretization_hh

#include "Spatial_Discretization.hh"

#include "Point.hh"

class Simple_Spatial_Discretization : public Spatial_Discretization
{
public:
    
    Simple_Spatial_Discretization(std::vector<std::shared_ptr<Point> > points);

    virtual bool has_reflection() const override
    {
        return false;
    }
    virtual int number_of_points() const override
    {
        return number_of_points_;
    }
    virtual int number_of_boundary_points() const override
    {
        return number_of_boundary_points_;
    }
    virtual int dimension() const override
    {
        return dimension_;
    }
    virtual int number_of_nodes() const override
    {
        return 1;
    }
    virtual std::shared_ptr<Dimensional_Moments> dimensional_moments() const override
    {
        return dimensional_moments_;
    }
    virtual std::shared_ptr<Point> point(int point_index) const override
    {
        return points_[point_index];
    }

    virtual void output(XML_Node output_node) const override;
    virtual void check_class_invariants() const override;
    
private:

    int dimension_;
    int number_of_points_;
    int number_of_boundary_points_;
    std::shared_ptr<Dimensional_Moments> dimensional_moments_;
    std::vector<std::shared_ptr<Point> > points_;
};

#endif
