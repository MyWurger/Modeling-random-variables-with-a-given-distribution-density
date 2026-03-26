#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class SamplingMethod
{
    InverseFunction = 0,
    Rejection = 1,
};

struct SamplingOptions
{
    SamplingMethod method = SamplingMethod::InverseFunction;
    std::size_t sampleSize = 10000;
    std::size_t histogramBins = 25;
    double integrationStep = 0.001;
    double alpha = 0.05;
};

struct HistogramBin
{
    double left = 0.0;
    double right = 0.0;
    std::size_t count = 0;
    double density = 0.0;
};

struct SamplingStatistics
{
    double mean = 0.0;
    double variance = 0.0;
    double standardDeviation = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    double median = 0.0;
};

struct KolmogorovResult
{
    double dn = 0.0;
    double kn = 0.0;
    double criticalValue = 0.0;
    bool rejectNullHypothesis = false;
};

struct SamplingResult
{
    bool success = false;
    std::string message;
    SamplingMethod method = SamplingMethod::InverseFunction;
    std::vector<double> samples;
    std::vector<double> sortedSamples;
    std::vector<HistogramBin> histogram;
    SamplingStatistics empiricalStatistics;
    KolmogorovResult kolmogorov;
    double acceptanceRate = -1.0;
};
