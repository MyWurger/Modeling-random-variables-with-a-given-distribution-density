#pragma once

#include "PlotChartWidget.h"
#include "SamplingTypes.h"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QAbstractTableModel;
class QComboBox;
class QDoubleSpinBox;
template <typename T>
class QFutureWatcher;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableView;
class QWidget;
QT_END_NAMESPACE

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void BuildUi();
    void ApplyTheme();
    void SetupSignals();
    void ResetOutputs();
    void RunSampling();
    bool ValidateInput(QString& errorMessage) const;
    void UpdateMethodSpecificInputsVisibility();
    void ApplySamplingRunningState(bool running);
    void FinishSampling(const SamplingResult& result);
    void PopulateTable(const SamplingResult& result);
    void UpdateSummary(const SamplingResult& result);
    void UpdatePlots(const SamplingResult& result);
    void ShowStatus(const QString& text, bool success);
    static QString FormatNumber(double value);

private:
    QComboBox* methodComboBox_ = nullptr;
    QSpinBox* sampleSizeEdit_ = nullptr;
    QSpinBox* binsEdit_ = nullptr;
    QDoubleSpinBox* stepEdit_ = nullptr;
    QDoubleSpinBox* alphaEdit_ = nullptr;
    QLabel* stepLabel_ = nullptr;
    QWidget* stepFieldContainer_ = nullptr;
    QLabel* rejectionTheoryLabel_ = nullptr;
    QLabel* rejectionTheoryValueLabel_ = nullptr;
    QPushButton* runButton_ = nullptr;
    QLabel* statusBadge_ = nullptr;

    QLabel* sampleCountValue_ = nullptr;
    QLabel* meanValue_ = nullptr;
    QLabel* varianceValue_ = nullptr;
    QLabel* dnValue_ = nullptr;
    QLabel* knValue_ = nullptr;
    QLabel* criticalValue_ = nullptr;
    QLabel* hypothesisValue_ = nullptr;
    QLabel* acceptanceValue_ = nullptr;

    PlotChartWidget* histogramPlot_ = nullptr;
    PlotChartWidget* cdfPlot_ = nullptr;
    PlotChartWidget* samplePlot_ = nullptr;

    QTabWidget* resultsTabs_ = nullptr;
    QTableView* samplesTable_ = nullptr;
    QAbstractTableModel* samplesModel_ = nullptr;

    SamplingResult lastResult_;
    bool hasResult_ = false;
    bool isSamplingRunning_ = false;
    QFutureWatcher<SamplingResult>* simulationWatcher_ = nullptr;
};
