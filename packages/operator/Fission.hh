#ifndef Fission_hh
#define Fission_hh

#include <memory>
#include <vector>

#include "Scattering_Operator.hh"

/*
  Applies fission to a moment representation of the flux
*/
class Fission : public Scattering_Operator
{
public:

    // Constructor
    Fission(std::shared_ptr<Spatial_Discretization> spatial_discretization,
            std::shared_ptr<Angular_Discretization> angular_discretization,
            std::shared_ptr<Energy_Discretization> energy_discretization,
            Scattering_Type scattering_type = Scattering_Type::FULL);
    
private: 

    // Apply within-group and out-of-group fission
    virtual void apply_full(std::vector<double> &x) const override;

    // Apply only within-group fission
    virtual void apply_coherent(std::vector<double> &x) const override;

    void calculate_cross_sections(std::vector<double> &chi,
                                  std::vector<double> &nu,
                                  std::vector<double> &sigma_f) const;
};

#endif
