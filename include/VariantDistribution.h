#pragma once

class TVariantDistribution
{
public:
    static double Density(double x) noexcept;
    static double Distribution(double x) noexcept;
    static double Quantile(double probability) noexcept;

    static constexpr double SupportMin() noexcept
    {
        return 0.0;
    }

    static constexpr double SupportMax() noexcept
    {
        return 1.0;
    }

    static constexpr double MaxDensity() noexcept
    {
        return 2.0;
    }

    static constexpr double Mean() noexcept
    {
        return 2.0 / 3.0;
    }

    static constexpr double Variance() noexcept
    {
        return 1.0 / 18.0;
    }

    static constexpr double StandardDeviation() noexcept
    {
        return 0.23570226039551584;
    }

    static constexpr double Median() noexcept
    {
        return 0.7071067811865476;
    }

    static constexpr double Mode() noexcept
    {
        return 1.0;
    }
};
