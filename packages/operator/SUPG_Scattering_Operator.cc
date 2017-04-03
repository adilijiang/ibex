#include "SUPG_Scattering_Operator.hh"

#include <memory>
#include <vector>

#include "Angular_Discretization.hh"
#include "Check.hh"
#include "Dimensional_Moments.hh"
#include "Energy_Discretization.hh"
#include "Spatial_Discretization.hh"

using namespace std;

SUPG_Scattering_Operator::
SUPG_Scattering_Operator(shared_ptr<Spatial_Discretization> spatial_discretization,
                    shared_ptr<Angular_Discretization> angular_discretization,
                    shared_ptr<Energy_Discretization> energy_discretization,
                    Options options):
    Vector_Operator(),
    spatial_discretization_(spatial_discretization),
    angular_discretization_(angular_discretization),
    energy_discretization_(energy_discretization),
    options_(options)
{
    shared_ptr<Dimensional_Moments> dimensional_moments
        = spatial_discretization->dimensional_moments();
    
    int phi_size = (spatial_discretization->number_of_points()
                    * spatial_discretization->number_of_nodes()
                    * energy_discretization->number_of_groups()
                    * angular_discretization->number_of_moments());

    column_size_ = phi_size * dimensional_moments->number_of_dimensional_moments();
    row_size_ = phi_size * dimensional_moments->number_of_double_dimensional_moments();
}

void SUPG_Scattering_Operator::
apply(vector<double> &x) const
{
    switch(options_.scattering_type)
    {
    case Options::Scattering_Type::FULL:
        apply_full(x);
        break;
    case Options::Scattering_Type::COHERENT:
        apply_coherent(x);
        break;
    case Options::Scattering_Type::INCOHERENT:
        apply_incoherent(x);
        break;
    }
}

void SUPG_Scattering_Operator::
apply_incoherent(vector<double> &x) const
{
    vector<double> y(x);
    
    apply_full(y);
    
    apply_coherent(x);

    for (int i = 0; i < row_size(); ++i)
    {
        x[i] = y[i] - x[i];
    }
}
