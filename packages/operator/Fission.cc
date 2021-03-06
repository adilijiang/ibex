#include "Fission.hh"

#if defined(ENABLE_OPENMP)
    #include <omp.h>
#endif

#include <memory>
#include <vector>

#include "Angular_Discretization.hh"
#include "Check.hh"
#include "Cross_Section.hh"
#include "Dimensional_Moments.hh"
#include "Energy_Discretization.hh"
#include "Material.hh"
#include "Point.hh"
#include "Spatial_Discretization.hh"

using namespace std;

Fission::
Fission(shared_ptr<Spatial_Discretization> spatial_discretization,
        shared_ptr<Angular_Discretization> angular_discretization,
        shared_ptr<Energy_Discretization> energy_discretization,
        Options options):
    Scattering_Operator(spatial_discretization,
                        angular_discretization,
                        energy_discretization,
                        options)
{
    check_class_invariants();
}

void Fission::
check_class_invariants() const
{
    Assert(spatial_discretization_);
    Assert(angular_discretization_);
    Assert(energy_discretization_);

    int number_of_points = spatial_discretization_->number_of_points();
    
    // For now, assume that all points have the same energy dependence
    Cross_Section::Dependencies::Energy energy_dep = spatial_discretization_->point(0)->material()->sigma_f()->dependencies().energy;
    for (int i = 0; i < number_of_points; ++i)
    {
        shared_ptr<Material> material = spatial_discretization_->point(i)->material();
        vector<Cross_Section::Dependencies> deps
            = {material->nu()->dependencies(),
               material->sigma_f()->dependencies(),
               material->chi()->dependencies()};
        
        for (Cross_Section::Dependencies &dep : deps)
        {
            Assert(dep.angular == Cross_Section::Dependencies::Angular::NONE);
        }
        if (energy_dep == Cross_Section::Dependencies::Energy::GROUP_TO_GROUP)
        {
            Assert(deps[1].energy == Cross_Section::Dependencies::Energy::GROUP_TO_GROUP);
        }
        else
        {
            for (Cross_Section::Dependencies &dep : deps)
            {
                Assert(dep.energy == Cross_Section::Dependencies::Energy::GROUP);
            }
        }
    }
}

void Fission::
apply_full(vector<double> &x) const
{
    switch (spatial_discretization_->point(0)->material()->sigma_f()->dependencies().energy)
    {
    case Cross_Section::Dependencies::Energy::GROUP:
        group_full(x);
        break;
    case Cross_Section::Dependencies::Energy::GROUP_TO_GROUP:
        group_to_group_full(x);
        break;
    default:
        Assert(false);
        break;
    }
}

void Fission::
apply_coherent(vector<double> &x) const
{
    switch (spatial_discretization_->point(0)->material()->sigma_f()->dependencies().energy)
    {
    case Cross_Section::Dependencies::Energy::GROUP:
        group_coherent(x);
        break;
    case Cross_Section::Dependencies::Energy::GROUP_TO_GROUP:
        group_to_group_coherent(x);
        break;
    default:
        Assert(false);
        break;
    }
}

void Fission::
group_to_group_full(vector<double> &x) const
{
    vector<double> y(x);

    int number_of_points = spatial_discretization_->number_of_points();
    int number_of_nodes = spatial_discretization_->number_of_nodes();
    int number_of_groups = energy_discretization_->number_of_groups();
    int number_of_moments = angular_discretization_->number_of_moments();
    int number_of_dimensional_moments = spatial_discretization_->dimensional_moments()->number_of_dimensional_moments();

    #pragma omp parallel for schedule(dynamic, 10)
    for (int i = 0; i < number_of_points; ++i)
    {
        shared_ptr<Cross_Section> sigma_f_cs = spatial_discretization_->point(i)->material()->sigma_f();
        vector<double> const sigma_f = sigma_f_cs->data();
        
        int m = 0;
        int d = 0;
        for (int gt = 0; gt < number_of_groups; ++gt)
        {
            for (int n = 0; n < number_of_nodes; ++n)
            {
                double sum = 0;
                    
                for (int gf = 0; gf < number_of_groups; ++gf)
                {
                    int k_phi_from = n + number_of_nodes * (gf + number_of_groups * (m + number_of_moments * i));
                    int k_sigma = d + number_of_dimensional_moments * (gf + number_of_groups * gt);
                    
                    sum += sigma_f[k_sigma] * y[k_phi_from];
                }
                            
                int k_phi_to = n + number_of_nodes * (gt + number_of_groups * (m + number_of_moments * i));
                    
                x[k_phi_to] = sum;
            }
        }
    }
    
    // Zero out other moments
    for (int i = 0; i < number_of_points; ++i)
    {
        for (int m = 1; m < number_of_moments; ++m)
        {
            for (int g = 0; g < number_of_groups; ++g)
            {
                for (int n = 0; n < number_of_nodes; ++n)
                {
                    int k_phi = n + number_of_nodes * (g + number_of_groups * (m + number_of_moments * i));
                        
                    x[k_phi] = 0;
                }
            }
        }
    }
}

void Fission::
group_to_group_coherent(vector<double> &x) const
{
    AssertMsg(false, "not implemented");
}

void Fission::
group_full(vector<double> &x) const
{
    int number_of_points = spatial_discretization_->number_of_points();
    int number_of_nodes = spatial_discretization_->number_of_nodes();
    int number_of_dimensional_moments = spatial_discretization_->dimensional_moments()->number_of_dimensional_moments();
    int number_of_groups = energy_discretization_->number_of_groups();
    int number_of_moments = angular_discretization_->number_of_moments();
    int number_of_ordinates = angular_discretization_->number_of_ordinates();
    
    {
        int m = 0;
        int d = 0;
        #pragma omp parallel for schedule(dynamic, 10)
        for (int i = 0; i < number_of_points; ++i)
        {
            shared_ptr<Material> material = spatial_discretization_->point(i)->material();
            vector<double> const nu = material->nu()->data();
            vector<double> const sigma_f = material->sigma_f()->data();
            vector<double> const chi = material->chi()->data();
            
            for (int n = 0; n < number_of_nodes; ++n)
            {
                // Calculate fission source
                
                double fission_source = 0;
                    
                for (int g = 0; g < number_of_groups; ++g)
                {
                    int k_phi = n + number_of_nodes * (g + number_of_groups * (m + number_of_moments * i));
                    int k_xs = d + number_of_dimensional_moments * g;
                    
                    fission_source += nu[k_xs] * sigma_f[k_xs] * x[k_phi];
                }
                    
                // Assign flux back to the appropriate group and zeroth moment
                for (int g = 0; g < number_of_groups; ++g)
                {
                    int k_phi = n + number_of_nodes * (g + number_of_groups * (m + number_of_moments * i));
                    int k_xs = d + number_of_dimensional_moments * g;
                    
                    x[k_phi] = chi[k_xs] * fission_source;
                }
            }
        }
    }
    
    // Zero out other moments
    for (int i = 0; i < number_of_points; ++i)
    {
        for (int m = 1; m < number_of_moments; ++m)
        {
            for (int g = 0; g < number_of_groups; ++g)
            {
                for (int n = 0; n < number_of_nodes; ++n)
                {
                    int k_phi = n + number_of_nodes * (g + number_of_groups * (m + number_of_moments * i));
                    
                    x[k_phi] = 0;
                }
            }
        }
    }
}

void Fission::
group_coherent(vector<double> &x) const
{
    int number_of_points = spatial_discretization_->number_of_points();
    int number_of_nodes = spatial_discretization_->number_of_nodes();
    int number_of_dimensional_moments = spatial_discretization_->dimensional_moments()->number_of_dimensional_moments();
    int number_of_groups = energy_discretization_->number_of_groups();
    int number_of_moments = angular_discretization_->number_of_moments();
    int number_of_ordinates = angular_discretization_->number_of_ordinates();

    // Apply within-group fission to zeroth moment
    {
        int m = 0;
        int d = 0;
        #pragma omp parallel for schedule(dynamic, 10)
        for (int i = 0; i < number_of_points; ++i)
        {
            shared_ptr<Material> material = spatial_discretization_->point(i)->material();
            vector<double> const nu = material->nu()->data();
            vector<double> const sigma_f = material->sigma_f()->data();
            vector<double> const chi = material->chi()->data();
            
            for (int g = 0; g < number_of_groups; ++g)
            {
                int k_xs = d + number_of_dimensional_moments * g;
                double cs = chi[k_xs] * nu[k_xs] * sigma_f[k_xs];
                
                for (int n = 0; n < number_of_nodes; ++n)
                {
                    int k_phi = n + number_of_nodes * (g + number_of_groups * (m + number_of_moments * i));
                    
                    x[k_phi] = cs * x[k_phi];
                }
            }
        }
    }
    
    // Zero out other moments
    for (int i = 0; i < number_of_points; ++i)
    {
        for (int m = 1; m < number_of_moments; ++m)
        {
            for (int g = 0; g < number_of_groups; ++g)
            {
                for (int n = 0; n < number_of_nodes; ++n)
                {
                    int k_phi = n + number_of_nodes * (g + number_of_groups * (m + number_of_moments * i));
                        
                    x[k_phi] = 0;
                }
            }
        }
    }
}
