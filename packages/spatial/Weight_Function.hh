#ifndef Weight_Function_hh
#define Weight_Function_hh

#include "Point.hh"

#include <functional>
#include <unordered_map>

class Basis_Function;
class Cartesian_Plane;
class Meshless_Function;
class Solid_Geometry;

class Weight_Function : public Point
{
public:

    enum Errors
    {
        DOES_NOT_EXIST = -1;
    };
    
    struct Options
    {
    public:

        // Main value to set: method of weighting
        enum class Weighting
        {
            POINT,
            WEIGHT,
            FLUX
        };
        
        // Determines whether to add the dimensional material moments
        enum class Output
        {
            STANDARD,
            SUPG
        };
        
        // Total cross section method: changed to ISOTROPIC
        // unless Weighting::FLUX is used
        enum class Total
        {
            ISOTROPIC,
            MOMENT
        };

        enum class Tau_Scaling
        {
            NONE,
            FUNCTIONAL, // 1 - b(boundary) - b(center)
            LINEAR, // pos_boundary / radius
            ABSOLUTE // 0 if on boundary
        };
        
        // Parameters that are automatically set
        // Don't use before Weight_Function is created
        bool include_supg = false;
        bool normalized = true;
        double tau; // SUPG parameter (tau_const / shape)
        bool external_integral_calculation = false;
        
        // Parameters for the user to set
        int integration_ordinates = 32; // Dimensional integration quadrature
        double tau_const = 1; // Constant in front of 1/shape
        Weighting weighting = Weighting::WEIGHT; 
        Total total = Total::ISOTROPIC;
        Output output = Output::STANDARD;
        Tau_Scaling tau_scaling = Tau_Scaling::LINEAR;
        std::function<double(int /*moment*/,
                             int /*group*/,
                             std::vector<double> const & /*position*/)> flux;
    };

    struct Integrals
    {
        // Surface integrals
        std::vector<double> is_w; // weight function: s
        std::vector<double> is_b_w; // weight/basis functions: s->i
        
        // Volume integrals
        std::vector<double> iv_w; // weight function: none
        std::vector<double> iv_dw; // derivative of weight function: d
        std::vector<double> iv_b_w; // basis function and weight function: i
        std::vector<double> iv_b_dw; // basis function and derivative of weight function: dw->i
        std::vector<double> iv_db_w; // weight function and derivative of basis function: db->i
        std::vector<double> iv_db_dw; // derivative of basis and weight functions: db->dw->i
    };

    struct Values
    {
        // Values
        std::vector<double> v_b; // basis function at weight center
        std::vector<double> v_db; // derivative of basis function at weight pcenter 
    };
    
    // Standard Constructor
    Weight_Function(int index,
                    int dimension,
                    Options options,
                    std::shared_ptr<Meshless_Function> meshless_function,
                    std::vector<std::shared_ptr<Basis_Function> > basis_functions,
                    std::shared_ptr<Solid_Geometry> solid_geometry,
                    std::vector<std::shared_ptr<Cartesian_Plane> > boundary_surfaces);

    // Constructor for precalculated integrals and material
    Weight_Function(int index,
                    int dimension,
                    Options options,
                    std::shared_ptr<Meshless_Function> meshless_funciton,
                    std::vector<std::shared_ptr<Basis_Function> > basis_functions,
                    std::shared_ptr<Solid_Geometry> solid_geometry,
                    std::vector<std::shared_ptr<Cartesian_Plane> > boundary_surfaces,
                    std::shared_ptr<Material> material,
                    Integrals const &integrals);
    
    // Point functions
    virtual int index() const override
    {
        return index_;
    }
    virtual int dimension() const override
    {
        return dimension_;
    }
    virtual int number_of_nodes() const override
    {
        return 1;
    }
    virtual Point_Type point_type() const override
    {
        return point_type_;
    }
    virtual std::vector<double> const &position() const override
    {
        return position_;
    }
    virtual std::shared_ptr<Material> material() const override
    {
        return material_;
    }
    virtual void output(XML_Node output_node) const override;
    virtual void check_class_invariants() const override;

    // Weight_Function functions
    virtual int number_of_basis_functions() const
    {
        return number_of_basis_functions_;
    }
    virtual int number_of_boundary_surfaces() const
    {
        return number_of_boundary_surfaces_;
    }
    virtual int number_of_dimensional_moments() const
    {
        return number_of_dimensional_moments_;
    }
    virtual double radius() const
    {
        return radius_;
    }
    virtual Options options() const
    {
        return options_;
    }
    virtual std::vector<int> const &basis_function_indices() const
    {
        return basis_function_indices_;
    }
    virtual std::shared_ptr<Meshless_Function> function() const
    {
        return meshless_function_;
    }
    virtual std::shared_ptr<Basis_Function> basis_function(int i) const
    {
        return basis_functions_[i];
    }
    virtual std::shared_ptr<Solid_Geometry> solid_geometry() const
    {
        return solid_geometry_;
    }
    virtual std::shared_ptr<Cartesian_Plane> boundary_surface(int i) const
    {
        return weighted_boundary_surfaces_[i];
    }

    // Get values or integrals
    virtual Integrals const &integrals() const
    {
        return integrals_;
    };
    virtual Values const &values() const
    {
        return values_;
    }
    
    // Quadrature methods
    virtual bool get_full_quadrature(std::vector<std::vector<double> > &ordinates,
                                     std::vector<double> &weights) const;
    virtual bool get_basis_quadrature(int i,
                                      std::vector<std::vector<double> > &ordinates,
                                      std::vector<double> &weights) const;
    virtual bool get_full_surface_quadrature(int s,
                                             std::vector<std::vector<double> > &ordinates,
                                             std::vector<double> &weights) const;
    virtual bool get_basis_surface_quadrature(int i,
                                              int s,
                                              std::vector<std::vector<double> > &ordinates,
                                              std::vector<double> &weights) const;

    // Set data
    virtual void set_integrals(Weight_Function::Integrals const &integrals,
                               std::shared_ptr<Material> material);
    
    // Get local basis function index from from global basis function index
    // Returns Errors::DOES_NOT_EXIST if none found
    virtual int local_basis_index(int global_index) const;
    
    // Get local surface index from global surface index
    
    
private:

    // Specific quadrature methods
    virtual bool get_full_quadrature_1d(std::vector<std::vector<double> > &ordinates,
                                        std::vector<double> &weights) const;
    virtual bool get_full_quadrature_2d(std::vector<std::vector<double> > &ordinates,
                                        std::vector<double> &weights) const;
    virtual bool get_basis_quadrature_1d(int i,
                                         std::vector<std::vector<double> > &ordinates,
                                         std::vector<double> &weights) const;
    virtual bool get_basis_quadrature_2d(int i,
                                         std::vector<std::vector<double> > &ordinates,
                                         std::vector<double> &weights) const;
    virtual bool get_full_surface_quadrature_2d(int s,
                                                std::vector<std::vector<double> > &ordinates,
                                                std::vector<double> &weights) const;
    virtual bool get_basis_surface_quadrature_2d(int i,
                                                 int s,
                                                 std::vector<std::vector<double> > &ordinates,
                                                 std::vector<double> &weights) const;
    
    // Integration methods
    virtual void set_options_and_limits();
    virtual void calculate_values();
    virtual void calculate_integrals();
    virtual void calculate_material();
    virtual void calculate_boundary_source();
    
    // Specific integration methods
    virtual void calculate_standard_point_material();
    virtual void calculate_standard_weight_material();
    virtual void calculate_supg_point_material();
    virtual void calculate_supg_weight_material();
    virtual void calculate_weight_boundary_source();
    
    // Point data
    int index_;
    int dimension_;
    Point_Type point_type_;
    std::vector<double> position_;
    std::shared_ptr<Material> material_;

    // Weight_Function data
    int number_of_basis_functions_;
    int number_of_boundary_surfaces_;
    int number_of_dimensional_moments_;
    double radius_;
    Options options_;
    std::vector<int> basis_function_indices_;
    std::shared_ptr<Meshless_Function> meshless_function_;
    std::vector<std::shared_ptr<Basis_Function> > basis_functions_;
    std::shared_ptr<Solid_Geometry> solid_geometry_;
    std::vector<std::shared_ptr<Cartesian_Plane> > boundary_surfaces_;
    std::vector<std::shared_ptr<Cartesian_Plane> > weighted_boundary_surfaces_;
    std::unordered_map<int, int> basis_global_indices_;
    std::vector<int> local_surface_indices_;
    
    // Calculated data
    std::vector<double> min_boundary_limits_;
    std::vector<double> max_boundary_limits_;

    // Values and integrals of data
    Integrals integrals_;
    Values values_;
};

#endif
