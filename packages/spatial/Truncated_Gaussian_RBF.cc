#include "Truncated_Gaussian_RBF.hh"

#include <cmath>

Truncated_Gaussian_RBF::
Truncated_Gaussian_RBF(double radius):
    radius_(radius)
{
}

double Truncated_Gaussian_RBF::
value(double r) const
{
    if (r < radius_)
    {
        return exp(-r * r);
    }
    else
    {
        return 0.;
    }
}

double Truncated_Gaussian_RBF::
d_value(double r) const
{
    if (r < radius_)
    {
        return -2 * r * exp(-r * r);
    }
    else
    {
        return 0.;
    }
}

double Truncated_Gaussian_RBF::
dd_value(double r) const
{
    if (r < radius_)
    {
        return (-2 + 4 * r * r) * exp(- r * r);
    }
    else
    {
        return 0.;
    }
}
