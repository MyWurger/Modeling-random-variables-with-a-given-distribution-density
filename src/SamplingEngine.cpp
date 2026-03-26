#include "SamplingEngine.h"

#include "VariantDistribution.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace
{
SamplingStatistics ComputeStatistics(const std::vector<double>& samples,
                                    const std::vector<double>& sortedSamples)
{
    if (samples.empty() || sortedSamples.empty() || samples.size() != sortedSamples.size())
    {
        throw std::invalid_argument("Выборка и её сортированная версия должны быть корректны");
    }

    SamplingStatistics stats;

    stats.minimum = *std::min_element(samples.begin(), samples.end());
    stats.maximum = *std::max_element(samples.begin(), samples.end());

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    stats.mean = sum / static_cast<double>(samples.size());

    double varianceAccumulator = 0.0;
    for (double value : samples)
    {
        const double delta = value - stats.mean;
        varianceAccumulator += delta * delta;
    }

    stats.variance = varianceAccumulator / static_cast<double>(samples.size());
    stats.standardDeviation = std::sqrt(stats.variance);

    const std::size_t middle = sortedSamples.size() / 2;
    if (sortedSamples.size() % 2 == 0)
    {
        stats.median = 0.5 * (sortedSamples[middle - 1] + sortedSamples[middle]);
    }
    else
    {
        stats.median = sortedSamples[middle];
    }

    return stats;
}

std::vector<double> GenerateInverseFunctionSamples(std::size_t sampleSize, double integrationStep)
{
    if (!(integrationStep > 0.0))
    {
        throw std::invalid_argument("Шаг интегрирования должен быть больше нуля");
    }

    std::vector<double> samples;
    samples.reserve(sampleSize);

    std::vector<double> cumulativeProbabilities;
    std::vector<double> gridValues;

    const double supportMin = TVariantDistribution::SupportMin();
    const double supportMax = TVariantDistribution::SupportMax();

    const std::size_t stepCount = std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil((supportMax - supportMin) / integrationStep)));
    cumulativeProbabilities.reserve(stepCount);
    gridValues.reserve(stepCount);

    double y = supportMin;
    double accumulatedProbability = 0.0;
    while (y < supportMax)
    {
        accumulatedProbability += TVariantDistribution::Density(y) * integrationStep;
        y = std::min(y + integrationStep, supportMax);
        cumulativeProbabilities.push_back(accumulatedProbability);
        gridValues.push_back(y);
    }

    std::mt19937_64 generator(std::random_device{}());
    std::uniform_real_distribution<double> unitDistribution(0.0, 1.0);

    for (std::size_t index = 0; index < sampleSize; ++index)
    {
        const double k = unitDistribution(generator);
        const auto it = std::lower_bound(cumulativeProbabilities.cbegin(),
                                         cumulativeProbabilities.cend(),
                                         k);
        if (it == cumulativeProbabilities.cend())
        {
            samples.push_back(supportMax);
            continue;
        }

        const std::size_t lookupIndex =
            static_cast<std::size_t>(std::distance(cumulativeProbabilities.cbegin(), it));
        samples.push_back(gridValues[lookupIndex]);
    }

    return samples;
}

std::pair<std::vector<double>, double> GenerateRejectionSamples(std::size_t sampleSize)
{
    std::vector<double> samples;
    samples.reserve(sampleSize);

    std::mt19937_64 generator(std::random_device{}());
    std::uniform_real_distribution<double> unitDistribution(0.0, 1.0);

    std::size_t attempts = 0;
    while (samples.size() < sampleSize)
    {
        ++attempts;

        const double y = unitDistribution(generator);
        const double z = unitDistribution(generator);

        if (z < y)
        {
            samples.push_back(y);
        }
    }

    const double acceptanceRate =
        static_cast<double>(samples.size()) / static_cast<double>(attempts);

    return {samples, acceptanceRate};
}

std::vector<HistogramBin> BuildHistogram(const std::vector<double>& samples, std::size_t bins)
{
    if (bins == 0)
    {
        throw std::invalid_argument("Число интервалов гистограммы должно быть больше нуля");
    }

    std::vector<HistogramBin> histogram;
    histogram.reserve(bins);

    const double width =
        (TVariantDistribution::SupportMax() - TVariantDistribution::SupportMin()) /
        static_cast<double>(bins);

    for (std::size_t index = 0; index < bins; ++index)
    {
        const double left = TVariantDistribution::SupportMin() + static_cast<double>(index) * width;
        histogram.push_back({left, left + width, 0, 0.0});
    }

    for (double value : samples)
    {
        const double shifted = value - TVariantDistribution::SupportMin();
        std::size_t binIndex = static_cast<std::size_t>(shifted / width);
        if (binIndex >= bins)
        {
            binIndex = bins - 1;
        }
        histogram[binIndex].count += 1;
    }

    for (HistogramBin& bin : histogram)
    {
        bin.density =
            static_cast<double>(bin.count) /
            (static_cast<double>(samples.size()) * width);
    }

    return histogram;
}

double KolmogorovDistributionCdf(double value)
{
    if (!(value > 0.0))
    {
        return 0.0;
    }

    double seriesSum = 0.0;
    for (int index = 1; index <= 100; ++index)
    {
        const double sign = (index % 2 == 1) ? 1.0 : -1.0;
        const double exponent = -2.0 * static_cast<double>(index * index) * value * value;
        const double term = sign * std::exp(exponent);
        seriesSum += term;

        if (std::abs(term) < 1e-12)
        {
            break;
        }
    }

    return std::clamp(1.0 - 2.0 * seriesSum, 0.0, 1.0);
}

double KolmogorovCriticalValue(double alpha)
{
    if (!(alpha > 0.0 && alpha < 1.0))
    {
        throw std::invalid_argument("Уровень значимости alpha должен принадлежать интервалу (0, 1)");
    }

    const double targetProbability = 1.0 - alpha;

    double left = 0.0;
    double right = 3.0;

    while (KolmogorovDistributionCdf(right) < targetProbability)
    {
        right *= 2.0;
    }

    for (int iteration = 0; iteration < 80; ++iteration)
    {
        const double middle = 0.5 * (left + right);
        if (KolmogorovDistributionCdf(middle) < targetProbability)
        {
            left = middle;
        }
        else
        {
            right = middle;
        }
    }

    return 0.5 * (left + right);
}

KolmogorovResult EvaluateKolmogorov(const std::vector<double>& sortedSamples, double alpha)
{
    if (sortedSamples.empty())
    {
        throw std::invalid_argument("Для критерия Колмогорова требуется непустая выборка");
    }

    const double sampleSize = static_cast<double>(sortedSamples.size());

    double dPlus = 0.0;
    double dMinus = 0.0;

    for (std::size_t index = 0; index < sortedSamples.size(); ++index)
    {
        const double x = sortedSamples[index];
        const double theoretical = TVariantDistribution::Distribution(x);
        const double empiricalUpper =
            static_cast<double>(index + 1) / sampleSize;
        const double empiricalLower =
            static_cast<double>(index) / sampleSize;

        dPlus = std::max(dPlus, empiricalUpper - theoretical);
        dMinus = std::max(dMinus, theoretical - empiricalLower);
    }

    KolmogorovResult result;
    result.dn = std::max(dPlus, dMinus);
    result.kn = std::sqrt(sampleSize) * result.dn;
    result.criticalValue = KolmogorovCriticalValue(alpha);
    result.rejectNullHypothesis = result.kn > result.criticalValue;
    return result;
}
} // namespace

double TVariantDistribution::Density(double x) noexcept
{
    if (x < SupportMin() || x > SupportMax())
    {
        return 0.0;
    }

    return 2.0 * x;
}

double TVariantDistribution::Distribution(double x) noexcept
{
    if (x <= SupportMin())
    {
        return 0.0;
    }

    if (x >= SupportMax())
    {
        return 1.0;
    }

    return x * x;
}

double TVariantDistribution::Quantile(double probability) noexcept
{
    if (probability <= 0.0)
    {
        return SupportMin();
    }

    if (probability >= 1.0)
    {
        return SupportMax();
    }

    return std::sqrt(probability);
}

SamplingResult RunSampling(const SamplingOptions& options)
{
    try
    {
        if (options.sampleSize == 0)
        {
            return {false, "Размер выборки должен быть больше нуля"};
        }

        if (options.histogramBins == 0)
        {
            return {false, "Число интервалов гистограммы должно быть больше нуля"};
        }

        SamplingResult result;
        result.success = true;
        result.method = options.method;

        if (options.method == SamplingMethod::InverseFunction)
        {
            result.samples =
                GenerateInverseFunctionSamples(options.sampleSize, options.integrationStep);
            result.acceptanceRate = -1.0;
            result.message = "Моделирование методом обратной функции завершено";
        }
        else
        {
            auto generated = GenerateRejectionSamples(options.sampleSize);
            result.samples = std::move(generated.first);
            result.acceptanceRate = generated.second;
            result.message = "Моделирование методом исключения завершено";
        }

        result.sortedSamples = result.samples;
        std::sort(result.sortedSamples.begin(), result.sortedSamples.end());

        result.histogram = BuildHistogram(result.samples, options.histogramBins);
        result.empiricalStatistics = ComputeStatistics(result.samples, result.sortedSamples);
        result.kolmogorov = EvaluateKolmogorov(result.sortedSamples, options.alpha);

        return result;
    }
    catch (const std::exception& exception)
    {
        return {false, exception.what()};
    }
}
