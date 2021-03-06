#include "Weak_Spatial_Discretization_Factory.hh"

#include <cmath>
#include <iostream>

#include "Basis_Function.hh"
#include "Cartesian_Distance.hh"
#include "Cartesian_Plane.hh"
#include "Check.hh"
#include "Constructive_Solid_Geometry.hh"
#include "Dimensional_Moments.hh"
#include "Distance.hh"
#include "KD_Tree.hh"
#include "Linear_MLS_Function.hh"
#include "Meshless_Function_Factory.hh"
#include "RBF.hh"
#include "RBF_Factory.hh"
#include "RBF_Function.hh"
#include "Weak_Spatial_Discretization.hh"
#include "Weight_Function.hh"

using namespace std;

Weak_Spatial_Discretization_Factory::
Weak_Spatial_Discretization_Factory(shared_ptr<Solid_Geometry> solid_geometry,
                                    vector<shared_ptr<Cartesian_Plane> > const &boundary_surfaces):
    solid_geometry_(solid_geometry),
    boundary_surfaces_(boundary_surfaces)
{
}

void Weak_Spatial_Discretization_Factory::
get_basis_functions(int number_of_points,
                    vector<shared_ptr<Meshless_Function> > const &functions,
                    vector<shared_ptr<Basis_Function> > &bases) const
{
    Assert(functions.size() == number_of_points);
    
    int dimension = solid_geometry_->dimension();
    
    // Get basis functions
    bases.resize(number_of_points);
    for (int i = 0; i < number_of_points; ++i)
    {
        // Find local boundaries
        shared_ptr<Meshless_Function> function = functions[i];
        vector<shared_ptr<Cartesian_Plane> > local_boundaries;
        meshless_factory_.get_boundary_surfaces(function,
                                                boundary_surfaces_,
                                                local_boundaries);
        bases[i]
            = make_shared<Basis_Function>(i,
                                          dimension,
                                          function,
                                          local_boundaries);
    }
}

void Weak_Spatial_Discretization_Factory::
get_weight_functions(int number_of_points,
                     shared_ptr<Weight_Function_Options> weight_options,
                     shared_ptr<Weak_Spatial_Discretization_Options> weak_options,
                     shared_ptr<Dimensional_Moments> dimensional_moments,
                     vector<vector<int> > const &neighbors,
                     vector<shared_ptr<Meshless_Function> > const &functions,
                     vector<shared_ptr<Basis_Function> > const &bases,
                     vector<shared_ptr<Weight_Function> > &weights) const
{
    Assert(neighbors.size() == number_of_points);
    Assert(bases.size() == number_of_points);
    
    int dimension = solid_geometry_->dimension();
    
    // Get basis functions
    weights.resize(number_of_points);
    for (int i = 0; i < number_of_points; ++i)
    {
        // Find local boundaries
        shared_ptr<Meshless_Function> function = functions[i];
        vector<shared_ptr<Cartesian_Plane> > local_boundaries;
        meshless_factory_.get_boundary_surfaces(function,
                                                boundary_surfaces_,
                                                local_boundaries);

        // Get local basis functions
        vector<int> const &local_neighbors = neighbors[i];
        int number_of_bases = local_neighbors.size();
        vector<shared_ptr<Basis_Function> > local_bases(number_of_bases);
        for (int j = 0; j < number_of_bases; ++j)
        {
            local_bases[j] = bases[local_neighbors[j]];
        }
        
        weights[i]
            = make_shared<Weight_Function>(i,
                                           dimension,
                                           weight_options,
                                           weak_options,
                                           function,
                                           local_bases,
                                           dimensional_moments,
                                           solid_geometry_,
                                           local_boundaries);
    }
}

shared_ptr<Weak_Spatial_Discretization> Weak_Spatial_Discretization_Factory::
get_simple_discretization(int num_dimensional_points,
                          double radius_num_intervals,
                          bool basis_mls,
                          bool weight_mls,
                          string basis_type,
                          string weight_type,
                          shared_ptr<Weight_Function_Options> weight_options,
                          shared_ptr<Weak_Spatial_Discretization_Options> weak_options) const
{
    int dimension = solid_geometry_->dimension();

    // Set Galerkin option
    switch (weak_options->identical_basis_functions)
    {
    case Weak_Spatial_Discretization_Options::Identical_Basis_Functions::AUTO:
        if (basis_mls == weight_mls && basis_type == weight_type)
        {
            weak_options->identical_basis_functions = Weak_Spatial_Discretization_Options::Identical_Basis_Functions::TRUE;
        }
        else
        {
            weak_options->identical_basis_functions = Weak_Spatial_Discretization_Options::Identical_Basis_Functions::FALSE;
        }
        break;
    default:
        break;
    }
    
    // Get dimensional moments
    shared_ptr<Dimensional_Moments> dimensional_moments
        = make_shared<Dimensional_Moments>(weak_options->include_supg,
                                           dimension);
    
    // Get points
    int number_of_points;
    vector<vector<double> > points;
    vector<int> dimensional_points(dimension, num_dimensional_points);
    vector<vector<double> > limits;
    meshless_factory_.get_boundary_limits(dimension,
                                          boundary_surfaces_,
                                          limits);
    meshless_factory_.get_cartesian_points(dimension,
                                           dimensional_points,
                                           limits,
                                           number_of_points,
                                           points);
    
    // Get KD tree
    shared_ptr<KD_Tree> kd_tree
        = make_shared<KD_Tree>(dimension,
                               number_of_points,
                               points);
    
    // Get RBF and distance
    RBF_Factory rbf_factory;
    shared_ptr<RBF> basis_rbf = rbf_factory.get_rbf(basis_type); 
    shared_ptr<RBF> weight_rbf = rbf_factory.get_rbf(basis_type);
    shared_ptr<Distance> distance
        = make_shared<Cartesian_Distance>(dimension);
    bool global_rbf = (basis_rbf->range() == RBF::Range::GLOBAL
                       || weight_rbf->range() == RBF::Range::GLOBAL);
    
    // Get neighbors
    double interval = points[1][0] - points[0][0];
    double radius = interval * radius_num_intervals;
    vector<double> radii(number_of_points, radius);
    vector<vector<int> > neighbors;
    vector<vector<double> > squared_distances;
    meshless_factory_.get_neighbors(kd_tree,
                                    global_rbf,
                                    dimension,
                                    number_of_points,
                                    radii,
                                    radii,
                                    points,
                                    neighbors,
                                    squared_distances);
    Assert(meshless_factory_.check_point_conditioning(number_of_points,
                                                      radii,
                                                      neighbors,
                                                      squared_distances));
    
    // Get RBF functions
    vector<shared_ptr<Meshless_Function> > meshless_basis;
    if (basis_mls)
    {
        // Get simple meshless functions
        vector<shared_ptr<Meshless_Function> > simple_functions;
        meshless_factory_.get_rbf_functions(number_of_points,
                                            radii,
                                            points,
                                            basis_rbf,
                                            distance,
                                            simple_functions);
        
        // Get MLS functions
        meshless_factory_.get_mls_functions(1,
                                            number_of_points,
                                            simple_functions,
                                            neighbors,
                                            meshless_basis);
    }
    else
    {
        meshless_factory_.get_rbf_functions(number_of_points,
                                            radii,
                                            points,
                                            basis_rbf,
                                            distance,
                                            meshless_basis);
    }
    vector<shared_ptr<Meshless_Function> > meshless_weight;
    if (weight_mls)
    {
        // Get simple meshless functions
        vector<shared_ptr<Meshless_Function> > simple_functions;
        meshless_factory_.get_rbf_functions(number_of_points,
                                            radii,
                                            points,
                                            weight_rbf,
                                            distance,
                                            simple_functions);

        // Get MLS functions
        meshless_factory_.get_mls_functions(1,
                                            number_of_points,
                                            simple_functions,
                                            neighbors,
                                            meshless_weight);
    }
    else
    {
        meshless_factory_.get_rbf_functions(number_of_points,
                                            radii,
                                            points,
                                            weight_rbf,
                                            distance,
                                            meshless_weight);
    }
    
    // Get basis functions
    vector<shared_ptr<Basis_Function> > bases;
    get_basis_functions(number_of_points,
                        meshless_basis,
                        bases);

    // Get weight functions
    vector<shared_ptr<Weight_Function> > weights;
    get_weight_functions(number_of_points,
                         weight_options,
                         weak_options,
                         dimensional_moments,
                         neighbors,
                         meshless_weight,
                         bases,
                         weights);

    // Get integration options
    weak_options->limits = limits;
    weak_options->solid = solid_geometry_;
    weak_options->dimensional_cells.assign(dimension, 2 * (num_dimensional_points - 1));

    // Create weak spatial discretization
    return make_shared<Weak_Spatial_Discretization>(bases,
                                                    weights,
                                                    dimensional_moments,
                                                    weak_options,
                                                    kd_tree);
}
