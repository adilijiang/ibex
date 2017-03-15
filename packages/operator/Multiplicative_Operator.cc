#include "Multiplicative_Operator.hh"

using std::vector;

Multiplicative_Operator::
Multiplicative_Operator(int size,
                        double scalar):
    Vector_Operator(size,
                    size),
    scalar_(scalar)
{
    check_class_invariants();
}

void Multiplicative_Operator::
apply(vector<double> &x) const
{
    for (int i = 0; i < column_size(); ++i)
    {
        x[i] *= scalar_;
    }
}

void Multiplicative_Operator::
check_class_invariants() const
{
}
