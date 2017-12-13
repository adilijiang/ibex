#include "Weak_RBF_Sweep.hh"

#include <iostream>
#if defined(ENABLE_OPENMP)
    #include <omp.h>
#endif

#include "Amesos.h"
#include "AztecOO.h"
#include "AztecOO_ConditionNumber.h"
#include "BelosSolverFactory.hpp"
#include "BelosEpetraAdapter.hpp"
#include "Epetra_CrsMatrix.h"
#include "Epetra_LinearProblem.h"
#include "Epetra_Map.h"
#include "Epetra_MpiComm.h"
#include "Epetra_MultiVector.h"
#include "Epetra_SerialComm.h"
#include "Epetra_Vector.h"
#include "Ifpack.h"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_RCPStdSharedPtrConversions.hpp"
#include "Teuchos_ArrayRCP.hpp"

#include "Angular_Discretization.hh"
#include "Basis_Function.hh"
#include "Boundary_Source.hh"
#include "Cartesian_Plane.hh"
#include "Check.hh"
#include "Conversion.hh"
#include "Cross_Section.hh"
#include "Dimensional_Moments.hh"
#include "Energy_Discretization.hh"
#include "Material.hh"
#include "Transport_Discretization.hh"
#include "XML_Node.hh"

using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

Weak_RBF_Sweep::
Weak_RBF_Sweep(Options options,
               shared_ptr<Weak_Spatial_Discretization> spatial_discretization,
               shared_ptr<Angular_Discretization> angular_discretization,
               shared_ptr<Energy_Discretization> energy_discretization,
               shared_ptr<Transport_Discretization> transport_discretization):
    Sweep_Operator(Sweep_Type::ORDINATE,
                   transport_discretization),
    options_(options),
    spatial_discretization_(spatial_discretization),
    angular_discretization_(angular_discretization),
    energy_discretization_(energy_discretization)
{
    switch (options_.solver)
    {
    case Options::Solver::AMESOS:
        solver_ = make_shared<Amesos_Solver>(*this);
        break;
    case Options::Solver::AMESOS_PARALLEL:
        solver_ = make_shared<Amesos_Parallel_Solver>(*this);
        break;
    case Options::Solver::AZTEC:
        solver_ = make_shared<Aztec_Solver>(*this);
        break;
    case Options::Solver::AZTEC_IFPACK:
        solver_ = make_shared<Aztec_Ifpack_Solver>(*this);
        break;
    case Options::Solver::BELOS:
        solver_ = make_shared<Belos_Solver>(*this);
        break;
    case Options::Solver::BELOS_IFPACK:
        solver_ = make_shared<Belos_Ifpack_Solver>(*this);
        break;
    }
    
    check_class_invariants();
}

void Weak_RBF_Sweep::
check_class_invariants() const
{
    Assert(spatial_discretization_);
    Assert(angular_discretization_);
    Assert(energy_discretization_);
    Assert(solver_);
}

void Weak_RBF_Sweep::
apply(vector<double> &x) const
{
    solver_->solve(x);
    update_augments(x);
}

void Weak_RBF_Sweep::
update_augments(vector<double> &x) const
{
    int number_of_boundary_points = spatial_discretization_->number_of_boundary_points();
    int number_of_ordinates = angular_discretization_->number_of_ordinates();
    int number_of_groups = energy_discretization_->number_of_groups();
    int psi_size = transport_discretization_->psi_size();
    bool has_reflection = transport_discretization_->has_reflection();

    // No augments if there is no reflection
    if (!has_reflection)
    {
        return;
    }

    // Iterate over boundary indices
    for (int b = 0; b < number_of_boundary_points; ++b)
    {
        // Get global index
        int i = spatial_discretization_->boundary_basis(b)->index();

        for (int o = 0; o < number_of_ordinates; ++o)
        {
            for (int g = 0; g < number_of_groups; ++g)
            {
                // Set boundary augment to current value of angular flux
                int k_b = psi_size + g + number_of_groups * (o + number_of_ordinates * b);
                int k_psi = g + number_of_groups * (o + number_of_ordinates * i);
                x[k_b] = x[k_psi];
            }
        }
    }
}

void Weak_RBF_Sweep::
get_rhs(int i,
        int o,
        int g,
        vector<double> const &x,
        double &value) const
{
    // Get data
    shared_ptr<Weight_Function> weight = spatial_discretization_->weight(i);
    vector<double> const &is_b_w = weight->integrals().is_b_w;
    vector<double> const direction = angular_discretization_->direction(o);
    int number_of_basis_functions = weight->number_of_basis_functions();
    int number_of_boundary_surfaces = weight->number_of_boundary_surfaces();
    int number_of_ordinates = angular_discretization_->number_of_ordinates();
    int number_of_groups = energy_discretization_->number_of_groups();
    int dimension = spatial_discretization_->dimension();
    int psi_size = transport_discretization_->psi_size();
    bool has_reflection = transport_discretization_->has_reflection();

    value = 0;
    // Add reflection and boundary source contribution
    {
        // Get sum of normals and integrals
        vector<double> sum(dimension, 0);
        for (int s = 0; s < number_of_boundary_surfaces; ++s)
        {
            shared_ptr<Cartesian_Plane> surface = weight->boundary_surface(s);
            int surface_dimension = surface->surface_dimension();
            double const normal = surface->normal();
            
            // Only for incoming surfaces
            double dot = normal * direction[surface_dimension];
            if (dot < 0)
            {
                shared_ptr<Boundary_Source> source = surface->boundary_source();
                double local_sum = 0;
                // Add reflection
                if (has_reflection)
                {
                    double const alpha = source->alpha()[g];
                    vector<double> normal_vec(dimension, 0);
                    normal_vec[surface_dimension] = normal;
                    int o_ref = angular_discretization_->reflect_ordinate(o,
                                                                          normal_vec);
                    
                    // Sum contributions of basis functions
                    for (int j = 0; j < number_of_basis_functions; ++j)
                    {
                        shared_ptr<Basis_Function> basis = weight->basis_function(j);
                        if (basis->point_type() == Basis_Function::Point_Type::BOUNDARY)
                        {
                            int aug_index = basis->boundary_index();
                            int is_index = s + number_of_boundary_surfaces * j;
                            int psi_index = psi_size + g + number_of_groups * (o_ref + number_of_ordinates * aug_index);
                            local_sum += is_b_w[is_index] * x[psi_index] * alpha;
                        }
                    }
                }
                
                // Add boundary source
                if (include_boundary_source_)
                {
                    int index = g + number_of_groups * o;
                    local_sum += source->data()[index];
                }

                // Add local sum into full normal sum
                sum[surface_dimension] += normal * local_sum;
            }
        }
        
        // Add dot product of sum and direction into value
        for (int d = 0; d < dimension; ++d)
        {
            value -= sum[d] * direction[d];
        }
    }
    
    // Add internal source (given contribution)
    int index = g + number_of_groups * (o + number_of_ordinates * i);
    value += x[index];
}

void Weak_RBF_Sweep::
get_matrix_row(int i, // weight function index (row)
               int o, // ordinate
               int g, // group
               vector<int> &indices, // column indices (global basis)
               vector<double> &values) const // column values
{
    // Get data
    shared_ptr<Weight_Function> weight = spatial_discretization_->weight(i);
    Weight_Function::Integrals const integrals = weight->integrals();
    vector<double> const &iv_w = integrals.iv_w;
    vector<double> const &iv_dw = integrals.iv_dw;
    vector<double> const &is_b_w = integrals.is_b_w;
    vector<double> const &iv_b_w = integrals.iv_b_w;
    vector<double> const &iv_b_dw = integrals.iv_b_dw;
    vector<double> const &iv_db_dw = integrals.iv_db_dw;
    vector<double> const direction = angular_discretization_->direction(o);
    shared_ptr<Dimensional_Moments> const dimensional_moments
        = spatial_discretization_->dimensional_moments();
    int const number_of_dimensional_moments = dimensional_moments->number_of_dimensional_moments();
    int const number_of_basis_functions = weight->number_of_basis_functions();
    vector<int> const basis_indices = weight->basis_function_indices();
    int const number_of_boundary_surfaces = weight->number_of_boundary_surfaces();
    int const dimension = spatial_discretization_->dimension();
    int const number_of_groups = energy_discretization_->number_of_groups();
    shared_ptr<Weak_Spatial_Discretization_Options> const weak_options
        = spatial_discretization_->options();
    shared_ptr<Weight_Function_Options> const weight_options
        = weight->options();
    shared_ptr<Material> const material = weight->material();
    shared_ptr<Cross_Section> const sigma_t_cs = material->sigma_t();
    shared_ptr<Cross_Section> const norm_cs = material->norm();
    vector<double> const sigma_t_data = sigma_t_cs->data();
    vector<double> const norm_data = norm_cs->data();
    
    bool const include_supg = weak_options->include_supg;
    bool const normalized = weak_options->normalized;
    double const tau = weight_options->tau;
    vector<double> const dimensional_coefficients
        = dimensional_moments->coefficients(tau,
                                            direction);

    // Get indices
    {
        vector<int> const indices_data = weight->basis_function_indices();
        indices = indices_data;
    }
    
    // Get values
    values.assign(number_of_basis_functions, 0);
    for (int j = 0; j < number_of_basis_functions; ++j) // basis function index
    {
        double &value = values[j];
        
        // Add streaming surface contribution
        {
            // Get sum of normals and integrals
            vector<double> sum(dimension, 0);
            for (int s = 0; s < number_of_boundary_surfaces; ++s)
            {
                shared_ptr<Cartesian_Plane> surface = weight->boundary_surface(s);
                int surface_dimension = surface->surface_dimension();
                double const normal = surface->normal();

                // Only for outgoing surfaces
                double dot = normal * direction[surface_dimension];
                if (dot > 0)
                {
                    int is_index = s + number_of_boundary_surfaces * j;
                    sum[surface_dimension] += normal * is_b_w[is_index];
                }
            }
            
            // Add dot product with direction into value
            for (int d = 0; d < dimension; ++d)
            {
                value += sum[d] * direction[d];
            }
        }
        
        // Add streaming volume contribution
        for (int d = 0; d < dimension; ++d)
        {
            int iv_index = d + dimension * j;
            value -= direction[d] * iv_b_dw[iv_index];
        }
        
        // Add streaming SUPG contribution
        if (include_supg)
        {
            for (int d1 = 0; d1 < dimension; ++d1)
            {
                double sum = 0;
                for (int d2 = 0; d2 < dimension; ++d2)
                {
                    int iv_index = d2 + dimension * (d1 + dimension * j);
                    sum += iv_db_dw[iv_index] * direction[d2];
                }
                value += tau * sum * direction[d1];
            }
        }

        // Add collision term
        switch (sigma_t_cs->dependencies().spatial)
        {
        case Cross_Section::Dependencies::Spatial::BASIS_WEIGHT:
        {
            double sum = 0;
            
            for (int d = 0; d < number_of_dimensional_moments; ++d)
            {
                int k_sigma = d + number_of_dimensional_moments * (g + number_of_groups * j);
                sum += dimensional_coefficients[d] * sigma_t_data[k_sigma];
            }
            
            value += sum;
            break;
        } // Spatial::BASIS_WEIGHT
        case Cross_Section::Dependencies::Spatial::BASIS:
        {
            int const b = basis_indices[j];
            shared_ptr<Material> const basis_material = spatial_discretization_->weight(b)->material();
            shared_ptr<Cross_Section> const basis_sigma_t_cs = basis_material->sigma_t();
            vector<double> const basis_sigma_t_data = basis_sigma_t_cs->data();

            double sum = 0;
            for (int d = 0; d < number_of_dimensional_moments; ++d)
            {
                int const k_sigma = d + number_of_dimensional_moments * g;
                double const mult = (d == 0
                                     ? iv_b_w[j]
                                     : iv_b_dw[d - 1 + dimension * j]);
                sum += dimensional_coefficients[d] * mult * basis_sigma_t_data[k_sigma];
            }
            
            value += sum;
            break;
        }
        case Cross_Section::Dependencies::Spatial::WEIGHT:
        {
            Assert(weak_options->total == Weak_Spatial_Discretization_Options::Total::ISOTROPIC); // moment method not yet implemented
            
            // Get total cross section: leave out higher moments for now
            double sigma_t = 0;
            for (int d = 0; d < number_of_dimensional_moments; ++d)
            {
                int const k_sigma = d + number_of_dimensional_moments * g;
                sigma_t += sigma_t_data[k_sigma] * dimensional_coefficients[d];
            }
            
            // Normalize total cross section if needed
            if (!normalized)
            {
                double norm = 0;
                switch (norm_cs->dependencies().energy)
                {
                case Cross_Section::Dependencies::Energy::NONE:
                    // Norm depends only on dimensional moment
                    for (int d = 0; d < number_of_dimensional_moments; ++d)
                    {
                        norm += norm_data[d] * dimensional_coefficients[d];
                    }
                    break;
                case Cross_Section::Dependencies::Energy::GROUP:
                    // Norm depends on dimensional moment, angular moment and group
                    // Ignore the angular moment for now
                    for (int d = 0; d < number_of_dimensional_moments; ++d)
                    {
                        int const k_norm = d + number_of_dimensional_moments * g;
                        norm += norm_data[k_norm] * dimensional_coefficients[d];
                    }
                    break;
                default:
                    AssertMsg(false, "norm dependency incorrect");
                    break;
                }
                sigma_t /= norm;
            } // if !normalized
    
            double sum = iv_b_w[j];
            if (include_supg)
            {
                for (int d = 0; d < dimension; ++d)
                {
                    int iv_index = d + dimension * j;
                    sum += tau * direction[d] * iv_b_dw[iv_index];
                }
            }
            
            value += sum * sigma_t;
            break;
        } // Spatial::WEIGHT
        } // switch Spatial
    } // basis functions
}

void Weak_RBF_Sweep::
output(XML_Node output_node) const
{
    output_node.set_attribute(options_.solver_conversion()->convert(options_.solver),
                              "solver");
}

void Weak_RBF_Sweep::
save_matrix_as_xml(int o,
                   int g,
                   XML_Node output_node) const
{
    // Create matrix node
    int number_of_points = spatial_discretization_->number_of_points();
    XML_Node matrix_node = output_node.append_child("matrix");
    matrix_node.set_attribute(o, "o");
    matrix_node.set_attribute(g, "g");
    matrix_node.set_attribute(number_of_points, "number_of_points");
    matrix_node.set_child_vector(spatial_discretization_->number_of_basis_functions(), "number_of_entries");
    
    for (int i = 0; i < number_of_points; ++i)
    {
        // Get row of matrix
        vector<int> indices;
        vector<double> values;
        get_matrix_row(i,
                       o,
                       g,
                       indices,
                       values);

        // Store row of matrix
        XML_Node row_node = matrix_node.append_child("row");
        row_node.set_attribute(i, "row_index");
        row_node.set_child_vector(indices,
                                  "column_indices");
        row_node.set_child_vector(values,
                                  "values");
    }
}

Weak_RBF_Sweep::Sweep_Solver::
Sweep_Solver(Weak_RBF_Sweep const &wrs):
    wrs_(wrs)
{
}

Weak_RBF_Sweep::Trilinos_Solver::
Trilinos_Solver(Weak_RBF_Sweep const &wrs):
    Sweep_Solver(wrs)
{
}

shared_ptr<Epetra_CrsMatrix> Weak_RBF_Sweep::Trilinos_Solver::
get_matrix(int o,
           int g,
           shared_ptr<Epetra_Map> map) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    vector<int> const number_of_basis_functions = wrs_.spatial_discretization_->number_of_basis_functions();
    
    shared_ptr<Epetra_CrsMatrix> mat
        = make_shared<Epetra_CrsMatrix>(Copy, // Data access
                                        *map,
                                        &number_of_basis_functions[0], // Num entries per row
                                        true); // Static profile
    for (int i = 0; i < number_of_points; ++i)
    {
        vector<int> indices;
        vector<double> values;
        wrs_.get_matrix_row(i,
                            o,
                            g,
                            indices,
                            values);
        mat->InsertGlobalValues(i, // Row
                                number_of_basis_functions[i], // Num entries
                                &values[0],
                                &indices[0]);
    }
    mat->FillComplete();
    mat->OptimizeStorage();
    
    return mat;
}

void Weak_RBF_Sweep::Trilinos_Solver::
set_rhs(int o,
        int g,
        std::shared_ptr<Epetra_Vector> &rhs,
        vector<double> const &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int k = g + number_of_groups * o;
    for (int i = 0; i < number_of_points; ++i)
    {
        double value;
        wrs_.get_rhs(i,
                     o,
                     g,
                     x,
                     value);
        
        (*rhs)[i] = value;
    }
}

void Weak_RBF_Sweep::Trilinos_Solver::
check_aztec_convergence(shared_ptr<AztecOO> const solver) const
{
    bool converged;
    string message;
    double const *status = solver->GetAztecStatus();
    switch ((int) status[AZ_why])
    {
    case AZ_normal:
        converged = true;
        break;
    case AZ_param:
        converged = false;
        message = "AztecOO: Parameter not available";
        break;
    case AZ_breakdown:
        converged = false;
        message = "AztecOO: Numerical breakdown";
        break;
    case AZ_loss:
        converged = false;
        message = "AztecOO: Numerical loss of precision";
        break;
    case AZ_ill_cond:
        converged = false;
        message = "AztecOO: Ill-conditioned matrix";
        break;
    case AZ_maxits:
        converged = false;
        message = "AztecOO: Max iterations reached without convergence";
        break;
    }
    if (!converged)
    {
        if (wrs_.options_.quit_if_diverged)
        {
            AssertMsg(false, message);
        }
        else
        {
            std::cerr << message << std::endl;
        }
    }
}

Weak_RBF_Sweep::Amesos_Solver::
Amesos_Solver(Weak_RBF_Sweep const &wrs):
    Trilinos_Solver(wrs)
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();
    
    // Initialize communication
    comm_ = make_shared<Epetra_SerialComm>();
    map_ = make_shared<Epetra_Map>(number_of_points, 0, *comm_);
    
    // Initialize matrices and vectors
    lhs_ = make_shared<Epetra_Vector>(*map_);
    rhs_ = make_shared<Epetra_Vector>(*map_);
    lhs_->PutScalar(1.0);
    rhs_->PutScalar(1.0);
    mat_.resize(number_of_groups * number_of_ordinates);
    problem_.resize(number_of_groups * number_of_ordinates);
    solver_.resize(number_of_groups * number_of_ordinates);
    Amesos factory;
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;

            mat_[k] = get_matrix(o,
                                 g,
                                 map_);
            problem_[k]
                = make_shared<Epetra_LinearProblem>(mat_[k].get(),
                                                    lhs_.get(),
                                                    rhs_.get());
            
            solver_[k]
                = shared_ptr<Amesos_BaseSolver>(factory.Create("Klu",
                                                               *problem_[k]));
                 
            AssertMsg(solver_[k]->SymbolicFactorization() == 0, "Amesos solver symbolic factorization failed");
            AssertMsg(solver_[k]->NumericFactorization() == 0, "Amesos solver numeric factorization failed");
        }
    }
}

void Weak_RBF_Sweep::Amesos_Solver::
solve(vector<double> &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();

    // Solve independently for each ordinate and group
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;
                
            // Set current RHS value
            set_rhs(o,
                    g,
                    rhs_,
                    x);
                
            // Solve, putting result into LHS
            AssertMsg(solver_[k]->Solve() == 0, "Amesos solver failed to solve");
            
            // Update solution value (overwrite x for this o and g)
            for (int i = 0; i < number_of_points; ++i)
            {
                int k_x = g + number_of_groups * (o + number_of_ordinates * i);
                x[k_x] = (*lhs_)[i];
            }
        }
    }
}

Weak_RBF_Sweep::Amesos_Parallel_Solver::
Amesos_Parallel_Solver(Weak_RBF_Sweep const &wrs):
    Trilinos_Solver(wrs)
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();
    
    // Initialize matrices
    comm_.resize(number_of_groups * number_of_ordinates);
    map_.resize(number_of_groups * number_of_ordinates);
    mat_.resize(number_of_groups * number_of_ordinates);
    lhs_.resize(number_of_groups * number_of_ordinates);
    rhs_.resize(number_of_groups * number_of_ordinates);
    problem_.resize(number_of_groups * number_of_ordinates);
    solver_.resize(number_of_groups * number_of_ordinates);
    Amesos factory;
    #pragma omp parallel for
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;

            comm_[k] = make_shared<Epetra_SerialComm>();
            map_[k] = make_shared<Epetra_Map>(number_of_points, 0, *comm_[k]);
            
            lhs_[k] = make_shared<Epetra_Vector>(*map_[k]);
            rhs_[k] = make_shared<Epetra_Vector>(*map_[k]);
            lhs_[k]->PutScalar(1.0);
            rhs_[k]->PutScalar(1.0);
            mat_[k] = get_matrix(o,
                                 g,
                                 map_[k]);
            problem_[k]
                = make_shared<Epetra_LinearProblem>(mat_[k].get(),
                                                    lhs_[k].get(),
                                                    rhs_[k].get());
            solver_[k]
                = shared_ptr<Amesos_BaseSolver>(factory.Create("Klu",
                                                               *problem_[k]));
                 
            AssertMsg(solver_[k]->SymbolicFactorization() == 0, "Amesos solver symbolic factorization failed");
            AssertMsg(solver_[k]->NumericFactorization() == 0, "Amesos solver numeric factorization failed");
        }
    }
}

void Weak_RBF_Sweep::Amesos_Parallel_Solver::
solve(vector<double> &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();

    // Solve independently for each ordinate and group
    #pragma omp parallel for
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;
                
            // Set current RHS value
            set_rhs(o,
                    g,
                    rhs_[k],
                    x);
                
            // Solve, putting result into LHS
            AssertMsg(solver_[k]->Solve() == 0, "Amesos solver failed to solve");
            
            // Update solution value (overwrite x for this o and g)
            for (int i = 0; i < number_of_points; ++i)
            {
                int k_x = g + number_of_groups * (o + number_of_ordinates * i);
                x[k_x] = (*lhs_[k])[i];
            }
        }
    }
}

Weak_RBF_Sweep::Aztec_Solver::
Aztec_Solver(Weak_RBF_Sweep const &wrs):
    Trilinos_Solver(wrs)
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    comm_ = make_shared<Epetra_MpiComm>(MPI_COMM_WORLD);
    map_ = make_shared<Epetra_Map>(number_of_points, 0, *comm_);
    lhs_ = make_shared<Epetra_Vector>(*map_);
    rhs_ = make_shared<Epetra_Vector>(*map_);
    lhs_->PutScalar(1.0);
    rhs_->PutScalar(1.0);
}

void Weak_RBF_Sweep::Aztec_Solver::
solve(vector<double> &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();

    // Solve independently for each ordinate and group
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            // Set current RHS value
            set_rhs(o,
                    g,
                    rhs_,
                    x);

            // Get matrix
            std::shared_ptr<Epetra_CrsMatrix> mat = get_matrix(o,
                                                               g,
                                                               map_);

            // Get linear problem
            int k = g + number_of_groups * o;
            std::shared_ptr<Epetra_LinearProblem> problem
                = make_shared<Epetra_LinearProblem>(mat.get(),
                                                    lhs_.get(),
                                                    rhs_.get());
            
            // Get solver
            shared_ptr<AztecOO> solver
                = make_shared<AztecOO>(*problem);
            solver->SetAztecOption(AZ_solver, AZ_gmres);
            solver->SetAztecOption(AZ_kspace, wrs_.options_.kspace);
            solver->SetAztecOption(AZ_precond, AZ_none);
            solver->SetAztecOption(AZ_output, AZ_warnings);
            
            // Solve, putting result into LHS
            solver->Iterate(wrs_.options_.max_iterations,
                            wrs_.options_.tolerance);

            // Check to ensure solver converged
            check_aztec_convergence(solver);
            
            // Update solution value (overwrite x for this o and g)
            for (int i = 0; i < number_of_points; ++i)
            {
                int k_x = g + number_of_groups * (o + number_of_ordinates * i);
                x[k_x] = (*lhs_)[i];
            }
        }
    }
}

Weak_RBF_Sweep::Aztec_Ifpack_Solver::
Aztec_Ifpack_Solver(Weak_RBF_Sweep const &wrs):
    Trilinos_Solver(wrs)
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();
    
    // Initialize communication
    comm_ = make_shared<Epetra_MpiComm>(MPI_COMM_WORLD);
    map_ = make_shared<Epetra_Map>(number_of_points, 0, *comm_);
    
    // Initialize matrices
    lhs_ = make_shared<Epetra_Vector>(*map_);
    rhs_ = make_shared<Epetra_Vector>(*map_);
    lhs_->PutScalar(1.0);
    rhs_->PutScalar(1.0);
    mat_.resize(number_of_groups * number_of_ordinates);
    problem_.resize(number_of_groups * number_of_ordinates);
    if (wrs_.options_.use_preconditioner)
    {
        prec_.resize(number_of_groups * number_of_ordinates);
    }
    solver_.resize(number_of_groups * number_of_ordinates);
    
    Ifpack ifp_factory;
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;

            // Get matrix and problem
            mat_[k] = get_matrix(o,
                                 g,
                                 map_);
            problem_[k]
                = make_shared<Epetra_LinearProblem>(mat_[k].get(),
                                                    lhs_.get(),
                                                    rhs_.get());

            if (wrs_.options_.use_preconditioner)
            {
                // Create preconditioner
                // ILU requires an int "fact: level-of-fill"
                // ILUT requires a double "fact: ilut level-of-fill"
                prec_[k]
                    = shared_ptr<Ifpack_Preconditioner>(ifp_factory.Create("ILUT",
                                                                           mat_[k].get()));
                Teuchos::ParameterList prec_list;
                prec_list.set("fact: drop tolerance", wrs_.options_.drop_tolerance);
                // prec_list.set("fact: level-of-fill", wrs_.options_.level_of_fill);
                prec_list.set("fact: ilut level-of-fill", wrs_.options_.level_of_fill);
                prec_[k]->SetParameters(prec_list);
                prec_[k]->Initialize();
                prec_[k]->Compute();
                Assert(prec_[k]->IsInitialized() == true);
                Assert(prec_[k]->IsComputed() == true);
            }
            
            // Initialize solver
            solver_[k] = make_shared<AztecOO>(*problem_[k]);
            solver_[k]->SetAztecOption(AZ_solver, AZ_gmres);
            solver_[k]->SetAztecOption(AZ_kspace, wrs_.options_.kspace);
            if (wrs_.options_.use_preconditioner)
            {
                solver_[k]->SetPrecOperator(prec_[k].get());
            }
            solver_[k]->SetAztecOption(AZ_output, AZ_warnings);
        }
    }
}

void Weak_RBF_Sweep::Aztec_Ifpack_Solver::
solve(vector<double> &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();

    // Solve independently for each ordinate and group
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;

            // Set the LHS value
            // If not set, the problem can converge too quickly
            // and the solver thinks it's incorrectly converged
            lhs_->PutScalar(1.0);
            
            // Set current RHS value
            set_rhs(o,
                    g,
                    rhs_,
                    x);
            
            // Solve, putting result into LHS
            solver_[k]->Iterate(wrs_.options_.max_iterations,
                                wrs_.options_.tolerance);
            
            // Check to ensure solver converged
            check_aztec_convergence(solver_[k]);
            
            // Update solution value (overwrite x for this o and g)
            for (int i = 0; i < number_of_points; ++i)
            {
                int k_x = g + number_of_groups * (o + number_of_ordinates * i);
                x[k_x] = (*lhs_)[i];
            }
        }
    }
}

Weak_RBF_Sweep::Belos_Solver::
Belos_Solver(Weak_RBF_Sweep const &wrs):
    Trilinos_Solver(wrs)
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    
    #pragma omp parallel
    {
        int number_of_threads = omp_get_num_threads();
        int t = omp_get_thread_num();

        #pragma omp single
        {
            // Initialize data pointers
            comm_.resize(number_of_threads);
            map_.resize(number_of_threads);
            lhs_.resize(number_of_threads);
            rhs_.resize(number_of_threads);
            problem_.resize(number_of_threads);
            solver_.resize(number_of_threads);
        }

        // Get comm and map
        comm_[t] = make_shared<Epetra_SerialComm>();
        map_[t] = make_shared<Epetra_Map>(number_of_points, 0, *comm_[t]);
        
        // Get vectors
        lhs_[t] = make_shared<Epetra_Vector>(*map_[t]);
        rhs_[t] = make_shared<Epetra_Vector>(*map_[t]);
        lhs_[t]->PutScalar(1.0);
        rhs_[t]->PutScalar(1.0);

        // Get problem and solver
        shared_ptr<Teuchos::ParameterList> belos_list
            = make_shared<Teuchos::ParameterList>();
        belos_list->set("Num Blocks", wrs_.options_.kspace);
        belos_list->set("Maximum Iterations", wrs_.options_.max_iterations);
        belos_list->set("Maximum Restarts", wrs_.options_.max_restarts);
        belos_list->set("Convergence Tolerance", wrs_.options_.tolerance);
        belos_list->set("Verbosity", Belos::Errors + Belos::Warnings);
        #pragma omp critical
        {
            problem_[t] = make_shared<BelosLinearProblem>();
            problem_[t]->setLHS(Teuchos::rcp(lhs_[t]));
            problem_[t]->setRHS(Teuchos::rcp(rhs_[t]));
            solver_[t] = make_shared<BelosSolver>(Teuchos::rcp(problem_[t]),
                                                  Teuchos::rcp(belos_list));
        }
    }
}

void Weak_RBF_Sweep::Belos_Solver::
solve(vector<double> &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();

    // Solve independently for each ordinate and group
    #pragma omp parallel
    {
        int number_of_threads = omp_get_num_threads();
        int t = omp_get_thread_num();
        Assert(problem_.size() == number_of_threads);
        
        #pragma omp for
        for (int o = 0; o < number_of_ordinates; ++o)
        {
            for (int g = 0; g < number_of_groups; ++g)
            {
                int k = g + number_of_groups * o;
                string description = std::to_string(o) + "_" + std::to_string(g);
                
                // Set current RHS value
                set_rhs(o,
                        g,
                        rhs_[t],
                        x);
                
                // Initialize LHS to 1.0 to avoid implicit residual problems
                lhs_[t]->PutScalar(1.0);

                // Get matrix
                shared_ptr<Epetra_CrsMatrix> mat
                    = get_matrix(o,
                                 g,
                                 map_[t]);

                // Set up problem
                problem_[t]->setOperator(Teuchos::rcp(mat));
                AssertMsg(problem_[t]->setProblem(), description);
            
                // Solve, putting result into LHS
                try
                {
                    Belos::ReturnType belos_result
                        = solver_[t]->solve();
                
                    if (wrs_.options_.quit_if_diverged)
                    {
                        AssertMsg(belos_result == Belos::Converged, description);
                    }
                }
                catch (Belos::StatusTestError const &error)
                {
                    AssertMsg(false, "Belos status test failed, " + description);
                }
                // std::cout << solver_[k]->getNumIters() << std::endl;
            
                // Update solution value (overwrite x for this o and g)
                for (int i = 0; i < number_of_points; ++i)
                {
                    int k_x = g + number_of_groups * (o + number_of_ordinates * i);
                    x[k_x] = (*lhs_[t])[i];
                }
            }
        }

    }
}

Weak_RBF_Sweep::Belos_Ifpack_Solver::
Belos_Ifpack_Solver(Weak_RBF_Sweep const &wrs):
    Trilinos_Solver(wrs)
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();
    
    // Initialize matrices
    comm_.resize(number_of_groups * number_of_ordinates);
    map_.resize(number_of_groups * number_of_ordinates);
    lhs_.resize(number_of_groups * number_of_ordinates);
    rhs_.resize(number_of_groups * number_of_ordinates);
    mat_.resize(number_of_groups * number_of_ordinates);
    if (wrs_.options_.use_preconditioner)
    {
        prec_.resize(number_of_groups * number_of_ordinates);
    }
    problem_.resize(number_of_groups * number_of_ordinates);
    solver_.resize(number_of_groups * number_of_ordinates);
    
    #pragma omp parallel for
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;
            string description = std::to_string(o) + "_" + std::to_string(g);

            // Get comm and map
            comm_[k] = make_shared<Epetra_SerialComm>();
            map_[k] = make_shared<Epetra_Map>(number_of_points, 0, *comm_[k]);

            // Get vectors and matrix
            lhs_[k] = make_shared<Epetra_Vector>(*map_[k]);
            rhs_[k] = make_shared<Epetra_Vector>(*map_[k]);
            lhs_[k]->PutScalar(1.0);
            rhs_[k]->PutScalar(1.0);
            mat_[k] = get_matrix(o,
                                 g,
                                 map_[k]);

            // Get preconditioner
            if (wrs_.options_.use_preconditioner)
            {
                Ifpack factory;
                shared_ptr<Ifpack_Preconditioner> temp_prec
                    = shared_ptr<Ifpack_Preconditioner>(factory.Create("ILUT",
                                                                       mat_[k].get()));
                Teuchos::ParameterList prec_list;
                prec_list.set("fact: drop tolerance", wrs_.options_.drop_tolerance);
                prec_list.set("fact: ilut level-of-fill", wrs_.options_.level_of_fill);
                temp_prec->SetParameters(prec_list);
                temp_prec->Initialize();
                temp_prec->Compute();
                AssertMsg(temp_prec->IsInitialized() == true, description);
                AssertMsg(temp_prec->IsComputed() == true, description);
                
                prec_[k]
                    = make_shared<BelosPreconditioner>(Teuchos::rcp(temp_prec));
            }

            // Get problem
            #pragma omp critical
            {
                problem_[k]
                    = make_shared<BelosLinearProblem>(Teuchos::rcp(mat_[k]),
                                                      Teuchos::rcp(lhs_[k]),
                                                      Teuchos::rcp(rhs_[k]));
                if (wrs_.options_.use_preconditioner)
                {
                    problem_[k]->setLeftPrec(Teuchos::rcp(prec_[k]));
                }
                AssertMsg(problem_[k]->setProblem(), description);
            }
            
            // Get solver
            shared_ptr<Teuchos::ParameterList> belos_list
                = make_shared<Teuchos::ParameterList>();
            belos_list->set("Num Blocks", wrs_.options_.kspace);
            belos_list->set("Maximum Iterations", wrs_.options_.max_iterations);
            belos_list->set("Maximum Restarts", wrs_.options_.max_restarts);
            belos_list->set("Convergence Tolerance", wrs_.options_.tolerance);
            belos_list->set("Verbosity", Belos::Errors + Belos::Warnings);
            #pragma omp critical
            {
                solver_[k]
                    = make_shared<BelosSolver>(Teuchos::rcp(problem_[k]),
                                               Teuchos::rcp(belos_list));
            }
        }
    }
}

void Weak_RBF_Sweep::Belos_Ifpack_Solver::
solve(vector<double> &x) const
{
    int number_of_points = wrs_.spatial_discretization_->number_of_points();
    int number_of_groups = wrs_.energy_discretization_->number_of_groups();
    int number_of_ordinates = wrs_.angular_discretization_->number_of_ordinates();

    // Solve independently for each ordinate and group
    #pragma omp parallel for
    for (int o = 0; o < number_of_ordinates; ++o)
    {
        for (int g = 0; g < number_of_groups; ++g)
        {
            int k = g + number_of_groups * o;
            string description = std::to_string(o) + "_" + std::to_string(g);
                
            // Set current RHS value
            set_rhs(o,
                    g,
                    rhs_[k],
                    x);

            // Initialize LHS to 1.0 to avoid implicit residual problems
            lhs_[k]->PutScalar(1.0);

            // Set up problem
            AssertMsg(problem_[k]->setProblem(), description);
            
            // Solve, putting result into LHS
            try
            {
                Belos::ReturnType belos_result
                    = solver_[k]->solve();
                
                if (wrs_.options_.quit_if_diverged)
                {
                    AssertMsg(belos_result == Belos::Converged, description);
                }
            }
            catch (Belos::StatusTestError const &error)
            {
                AssertMsg(false, "Belos status test failed, " + description);
            }
            // std::cout << solver_[k]->getNumIters() << std::endl;
            
            // Update solution value (overwrite x for this o and g)
            for (int i = 0; i < number_of_points; ++i)
            {
                int k_x = g + number_of_groups * (o + number_of_ordinates * i);
                x[k_x] = (*lhs_[k])[i];
            }
        }
    }
}

shared_ptr<Conversion<Weak_RBF_Sweep::Options::Solver, string> > Weak_RBF_Sweep::Options::
solver_conversion() const
{
    vector<pair<Solver, string> > conversions
        = {{Solver::AMESOS, "amesos"},
           {Solver::AMESOS_PARALLEL, "amesos_parallel"},
           {Solver::AZTEC, "aztec"},
           {Solver::AZTEC_IFPACK, "aztec_ifpack"},
           {Solver::BELOS, "belos"},
           {Solver::BELOS_IFPACK, "belos_ifpack"}};
           
    return make_shared<Conversion<Solver, string> >(conversions);
}
