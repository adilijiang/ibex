#ifndef RBF_Collocation_Sweep_hh
#define RBF_Collocation_Sweep_hh

#include <memory>

#include "RBF_Discretization.hh"
#include "Sweep_Operator.hh"

class Amesos_BaseSolver;
class AztecOO;
class Epetra_CrsMatrix;
class Epetra_Comm;
class Epetra_LinearProblem;
class Epetra_Map;
class Epetra_SerialDenseMatrix;
class Epetra_SerialDenseSolver;
class Epetra_Vector;

class RBF_Collocation_Sweep : public Sweep_Operator
{
public:

    enum class Matrix_Solver
    {
        AMESOS,
        AZTEC
    };
    
    enum class Solution_Variable
    {
        COEFFICIENT,
        PSI,
    };

    enum class Condition_Calculation
    {
        NONE,
        CHEAP,
        EXPENSIVE,
        AZTEC
    };
    
    RBF_Collocation_Sweep(Solution_Variable solution_variable,
                          Matrix_Solver matrix_solver,
                          Condition_Calculation condition_calculation,
                          std::shared_ptr<RBF_Discretization> rbf_discretization,
                          std::shared_ptr<Angular_Discretization> angular_discretization,
                          std::shared_ptr<Energy_Discretization> energy_discretization,
                          std::shared_ptr<Transport_Discretization> transport_discretization);

    virtual std::shared_ptr<RBF_Discretization> rbf_discretization() const
    {
        return rbf_discretization_;
    }
    virtual std::shared_ptr<Spatial_Discretization> spatial_discretization() const override
    {
        return rbf_discretization_;
    }
    virtual std::shared_ptr<Angular_Discretization> angular_discretization() const override
    {
        return angular_discretization_;
    }
    virtual std::shared_ptr<Energy_Discretization> energy_discretization() const override
    {
        return energy_discretization_;
    }

    virtual void output(pugi::xml_node output_node) const override;
    virtual void check_class_invariants() const override;

private:

    virtual void apply(std::vector<double> &x) const override;
    
    void calculate_condition_numbers();
    void initialize_trilinos();
    void set_point(int i,
                   int o,
                   int g);
    void set_boundary_point(int i,
                            int o,
                            int g);
    void set_internal_point(int i,
                            int o,
                            int g);
    void set_boundary_rhs(int b,
                          int i,
                          int o,
                          int g,
                          std::vector<double> const &x) const;
    void set_internal_rhs(int i,
                          int o,
                          int g,
                          std::vector<double> const &x) const;

    void convert_to_psi(int i,
                        int g,
                        int o,
                        std::vector<double> &data) const;
    
    void update_local_matrix(int i,
                             int g,
                             int o);
    void check_local_matrix(int i,
                            int g,
                            int o);
    
    Solution_Variable solution_variable_;
    Condition_Calculation condition_calculation_;
    Matrix_Solver matrix_solver_;
    std::shared_ptr<RBF_Discretization> rbf_discretization_;
    std::shared_ptr<Angular_Discretization> angular_discretization_;
    std::shared_ptr<Energy_Discretization> energy_discretization_;

    double reflection_tolerance_;

    std::shared_ptr<Epetra_Comm> comm_;
    std::shared_ptr<Epetra_Map> map_;
    std::shared_ptr<Epetra_Vector> lhs_;
    std::shared_ptr<Epetra_Vector> rhs_;
    std::vector<std::shared_ptr<Epetra_CrsMatrix> > mat_;
    std::vector<std::shared_ptr<Epetra_LinearProblem> > problem_;

    int max_iterations_;
    double tolerance_;
    mutable std::vector<int> num_calls_;
    mutable std::vector<int> num_iterations_;
    std::vector<std::shared_ptr<AztecOO> > aztec_solver_;
    std::vector<std::shared_ptr<Amesos_BaseSolver*> > amesos_solver_;
    
    std::vector<double> condition_numbers_;
    
    std::shared_ptr<Epetra_SerialDenseMatrix> local_mat_;
    std::shared_ptr<Epetra_SerialDenseSolver> local_solver_;
    mutable std::vector<int> num_averages_;
    mutable std::vector<double> average_local_condition_numbers_;
};

#endif
