#include "MainWindow.h"

#include "SamplingEngine.h"
#include "VariantDistribution.h"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QStringList>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QValidator>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
constexpr int kMaxTablePreviewRows = 5000;
constexpr std::size_t kMaxEmpiricalPlotSteps = 12000;
constexpr int kMaxRecommendedHistogramBins = 2000;
constexpr int kMaxRecommendedSampleSize = 5000000;

QString FormatNumericValue(double value);

class MethodComboBox final : public QComboBox
{
public:
    explicit MethodComboBox(QWidget* parent = nullptr) : QComboBox(parent)
    {
        setProperty("popupOpen", false);
    }

protected:
    void showPopup() override
    {
        setProperty("popupOpen", true);
        style()->unpolish(this);
        style()->polish(this);
        update();
        QComboBox::showPopup();
    }

    void hidePopup() override
    {
        QComboBox::hidePopup();
        setProperty("popupOpen", false);
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
};

class IntegerSpinBox final : public QSpinBox
{
public:
    explicit IntegerSpinBox(QWidget* parent = nullptr) : QSpinBox(parent)
    {
        setRange(1, 1000000);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setAccelerated(true);
        setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        setGroupSeparatorShown(false);
    }

    void SetPlaceholderText(const QString& text)
    {
        if (auto* edit = lineEdit())
        {
            edit->setPlaceholderText(text);
        }
    }

    bool HasInput() const
    {
        const auto* edit = lineEdit();
        return edit != nullptr && !edit->text().trimmed().isEmpty();
    }

protected:
    void focusOutEvent(QFocusEvent* event) override
    {
        const bool wasEmpty = !HasInput();
        QSpinBox::focusOutEvent(event);
        if (wasEmpty)
        {
            clear();
        }
    }

    void stepBy(int steps) override
    {
        if (!HasInput())
        {
            setValue(minimum());
        }
        QSpinBox::stepBy(steps);
    }
};

class DecimalSpinBox final : public QDoubleSpinBox
{
public:
    explicit DecimalSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent)
    {
        setDecimals(6);
        setSingleStep(1.0);
        setRange(-1.0e15, 1.0e15);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        setAccelerated(true);
        setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        setGroupSeparatorShown(false);
    }

    void SetPlaceholderText(const QString& text)
    {
        if (auto* edit = lineEdit())
        {
            edit->setPlaceholderText(text);
        }
    }

    bool HasInput() const
    {
        const auto* edit = lineEdit();
        return edit != nullptr && !edit->text().trimmed().isEmpty();
    }

protected:
    QString textFromValue(double value) const override
    {
        return FormatNumericValue(value);
    }

    double valueFromText(const QString& text) const override
    {
        QString normalized = text.trimmed();
        normalized.replace(',', '.');

        bool ok = false;
        const double value = normalized.toDouble(&ok);
        return ok ? value : 0.0;
    }

    QValidator::State validate(QString& text, int& pos) const override
    {
        Q_UNUSED(pos);
        QString normalized = text.trimmed();

        if (normalized.isEmpty() || normalized == "-" || normalized == "+" || normalized == "." || normalized == ",")
        {
            return QValidator::Intermediate;
        }

        normalized.replace(',', '.');
        bool ok = false;
        normalized.toDouble(&ok);
        return ok ? QValidator::Acceptable : QValidator::Invalid;
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        const bool wasEmpty = !HasInput();
        QDoubleSpinBox::focusOutEvent(event);
        if (wasEmpty)
        {
            clear();
        }
    }

    void stepBy(int steps) override
    {
        if (!HasInput())
        {
            setValue(0.0);
        }
        QDoubleSpinBox::stepBy(steps);
    }
};

QString FormatNumericValue(double value)
{
    if (!std::isfinite(value))
    {
        return "не число";
    }

    if (value == 0.0)
    {
        return "0";
    }

    const double absoluteValue = std::abs(value);
    if (absoluteValue < 1e-4 || absoluteValue >= 1e8)
    {
        return QString::number(value, 'e', 5);
    }

    QString text = QString::number(value, 'f', 6);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.')))
    {
        text.chop(1);
    }

    if (text == "-0")
    {
        return "0";
    }

    return text;
}

template <typename TWidget>
void ApplyInputPalette(TWidget* widget)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::Text, QColor("#102033"));
    palette.setColor(QPalette::WindowText, QColor("#102033"));
    palette.setColor(QPalette::ButtonText, QColor("#102033"));
    palette.setColor(QPalette::Highlight, QColor("#2a6df4"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7a8a9c"));
    widget->setPalette(palette);
}

QWidget* CreateSpinField(QSpinBox*& spinBox, int initialValue, const QString& placeholder, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    spinBox = new IntegerSpinBox(container);
    spinBox->setValue(initialValue);
    static_cast<IntegerSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    auto* buttonsHost = new QWidget(container);
    buttonsHost->setObjectName("spinButtonColumn");
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(4);

    auto* plusButton = new QPushButton("+", buttonsHost);
    auto* minusButton = new QPushButton("-", buttonsHost);
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");
    plusButton->setFixedSize(30, 18);
    minusButton->setFixedSize(30, 18);

    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepUp(); });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepDown(); });

    buttonsLayout->addWidget(plusButton);
    buttonsLayout->addWidget(minusButton);
    buttonsLayout->addStretch();

    layout->addWidget(spinBox, 1);
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);
    return container;
}

QWidget* CreateSpinField(QDoubleSpinBox*& spinBox, double initialValue, const QString& placeholder, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    spinBox = new DecimalSpinBox(container);
    spinBox->setValue(initialValue);
    static_cast<DecimalSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    auto* buttonsHost = new QWidget(container);
    buttonsHost->setObjectName("spinButtonColumn");
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(4);

    auto* plusButton = new QPushButton("+", buttonsHost);
    auto* minusButton = new QPushButton("-", buttonsHost);
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");
    plusButton->setFixedSize(30, 18);
    minusButton->setFixedSize(30, 18);

    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepUp(); });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepDown(); });

    buttonsLayout->addWidget(plusButton);
    buttonsLayout->addWidget(minusButton);
    buttonsLayout->addStretch();

    layout->addWidget(spinBox, 1);
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);
    return container;
}

QString MethodTitle(SamplingMethod method)
{
    return method == SamplingMethod::InverseFunction ? "Метод обратной функции"
                                                     : "Метод исключения";
}

void SetFieldContainerEnabled(QWidget* field, bool enabled)
{
    if (field == nullptr)
    {
        return;
    }

    if (auto* container = field->parentWidget())
    {
        container->setEnabled(enabled);
    }
    else
    {
        field->setEnabled(enabled);
    }
}

QVector<QPointF> BuildHistogramOutline(const std::vector<HistogramBin>& histogram)
{
    QVector<QPointF> points;
    if (histogram.empty())
    {
        return points;
    }

    points.reserve(static_cast<int>(histogram.size() * 4 + 2));
    points.append(QPointF(histogram.front().left, 0.0));

    for (const HistogramBin& bin : histogram)
    {
        points.append(QPointF(bin.left, bin.density));
        points.append(QPointF(bin.right, bin.density));
        points.append(QPointF(bin.right, 0.0));
    }

    return points;
}

QVector<QPointF> BuildDensityCurvePoints()
{
    QVector<QPointF> points;
    constexpr int kPointCount = 240;
    points.reserve(kPointCount + 1);

    for (int index = 0; index <= kPointCount; ++index)
    {
        const double x = static_cast<double>(index) / static_cast<double>(kPointCount);
        points.append(QPointF(x, TVariantDistribution::Density(x)));
    }

    return points;
}

QVector<QPointF> BuildTheoreticalDistributionPoints()
{
    QVector<QPointF> points;
    constexpr int kPointCount = 240;
    points.reserve(kPointCount + 1);

    for (int index = 0; index <= kPointCount; ++index)
    {
        const double x = static_cast<double>(index) / static_cast<double>(kPointCount);
        points.append(QPointF(x, TVariantDistribution::Distribution(x)));
    }

    return points;
}

QVector<QPointF> BuildEmpiricalDistributionPoints(const std::vector<double>& sortedSamples)
{
    QVector<QPointF> points;
    if (sortedSamples.empty())
    {
        return points;
    }

    const std::size_t rowStep =
        sortedSamples.size() <= kMaxEmpiricalPlotSteps
            ? 1
            : ((sortedSamples.size() - 1) / (kMaxEmpiricalPlotSteps - 1) + 1);
    const std::size_t estimatedSteps = (sortedSamples.size() - 1) / rowStep + 1;

    points.reserve(static_cast<int>(estimatedSteps * 2 + 2));
    points.append(QPointF(TVariantDistribution::SupportMin(), 0.0));

    const double sampleSize = static_cast<double>(sortedSamples.size());
    for (std::size_t index = 0; index < sortedSamples.size(); index += rowStep)
    {
        const double x = sortedSamples[index];
        const double lowerValue = static_cast<double>(index) / sampleSize;
        const double upperValue = static_cast<double>(index + 1) / sampleSize;

        points.append(QPointF(x, lowerValue));
        points.append(QPointF(x, upperValue));
    }

    if (((sortedSamples.size() - 1) % rowStep) != 0)
    {
        const std::size_t lastIndex = sortedSamples.size() - 1;
        const double x = sortedSamples[lastIndex];
        const double lowerValue = static_cast<double>(lastIndex) / sampleSize;
        points.append(QPointF(x, lowerValue));
        points.append(QPointF(x, 1.0));
    }

    points.append(QPointF(TVariantDistribution::SupportMax(), 1.0));
    return points;
}

QVector<QPointF> BuildSampleSequence(const std::vector<double>& samples)
{
    QVector<QPointF> points;
    if (samples.empty())
    {
        return points;
    }

    const std::size_t maxPoints = 5000;
    const std::size_t step =
        samples.size() <= maxPoints ? 1 : ((samples.size() - 1) / (maxPoints - 1) + 1);

    const std::size_t estimatedSize = (samples.size() - 1) / step + 1;
    points.reserve(static_cast<int>(estimatedSize + 1));

    for (std::size_t index = 0; index < samples.size(); index += step)
    {
        points.append(QPointF(static_cast<double>(index + 1), samples[index]));
    }

    if (points.isEmpty() || points.back().x() != static_cast<double>(samples.size()))
    {
        points.append(QPointF(static_cast<double>(samples.size()), samples.back()));
    }

    return points;
}

class SampleTableModel final : public QAbstractTableModel
{
public:
    explicit SampleTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent)
    {
    }

    void SetResult(const SamplingResult* result, int maxVisibleRows = -1)
    {
        beginResetModel();
        result_ = result;
        rowStep_ = 1;

        if (result_ != nullptr && maxVisibleRows > 0 &&
            result_->samples.size() > static_cast<std::size_t>(maxVisibleRows))
        {
            rowStep_ =
                (result_->samples.size() - 1) / static_cast<std::size_t>(maxVisibleRows - 1) + 1;
        }

        endResetModel();
    }

    bool IsSampled() const noexcept
    {
        return rowStep_ > 1;
    }

    std::size_t VisibleRowStep() const noexcept
    {
        return rowStep_;
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid() || result_ == nullptr)
        {
            return 0;
        }

        return static_cast<int>((result_->samples.size() - 1) / rowStep_ + 1);
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 4;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || result_ == nullptr)
        {
            return {};
        }

        const std::size_t sampleIndex =
            std::min(static_cast<std::size_t>(index.row()) * rowStep_,
                     result_->samples.size() - static_cast<std::size_t>(1));

        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }

        if (role != Qt::DisplayRole)
        {
            return {};
        }

        switch (index.column())
        {
        case 0:
            return static_cast<qulonglong>(sampleIndex + 1);
        case 1:
            return FormatNumericValue(result_->samples[sampleIndex]);
        case 2:
            return FormatNumericValue(result_->sortedSamples[sampleIndex]);
        case 3:
            return FormatNumericValue(static_cast<double>(sampleIndex) /
                                      static_cast<double>(result_->sortedSamples.size()));
        default:
            return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }

        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        {
            return QAbstractTableModel::headerData(section, orientation, role);
        }

        static const QStringList headers = {
            "i",
            "xᵢ",
            "x₍ᵢ₎",
            "Fэмп(x₍ᵢ₎)"};

        if (section < 0 || section >= headers.size())
        {
            return {};
        }

        return headers[section];
    }

private:
    const SamplingResult* result_ = nullptr;
    std::size_t rowStep_ = 1;
};
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    BuildUi();
    ApplyTheme();
    simulationWatcher_ = new QFutureWatcher<SamplingResult>(this);
    SetupSignals();
    ResetOutputs();
    ShowStatus("Готово к моделированию", true);
}

void MainWindow::BuildUi()
{
    setWindowTitle("Лабораторная работа 1 — моделирование случайной величины");
    resize(1560, 940);
    setMinimumSize(1320, 820);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(4);

    auto* controlCard = new QWidget(splitter);
    controlCard->setObjectName("controlCard");
    controlCard->setMinimumWidth(460);
    controlCard->setMaximumWidth(560);

    auto* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(18, 18, 18, 18);
    controlLayout->setSpacing(12);

    auto* titleLabel = new QLabel("Моделирование случайной величины", controlCard);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setWordWrap(true);

    auto* parametersGroup = new QGroupBox("Параметры моделирования", controlCard);
    parametersGroup->setObjectName("parametersGroup");
    auto* parametersForm = new QFormLayout(parametersGroup);
    parametersForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    parametersForm->setFormAlignment(Qt::AlignTop);
    parametersForm->setContentsMargins(14, 18, 14, 14);
    parametersForm->setSpacing(10);
    parametersForm->setHorizontalSpacing(16);

    methodComboBox_ = new MethodComboBox(parametersGroup);
    methodComboBox_->setObjectName("methodComboBox");
    methodComboBox_->setMinimumHeight(42);
    methodComboBox_->addItem("Метод обратной функции");
    methodComboBox_->addItem("Метод исключения");

    auto* sampleSizeField = CreateSpinField(sampleSizeEdit_, 10000, "10000", parametersGroup);
    sampleSizeEdit_->setRange(100, kMaxRecommendedSampleSize);
    sampleSizeEdit_->setSingleStep(5000);
    sampleSizeEdit_->setValue(10000);

    auto* binsField = CreateSpinField(binsEdit_, 25, "25", parametersGroup);
    binsEdit_->setRange(5, kMaxRecommendedHistogramBins);
    binsEdit_->setSingleStep(10);
    binsEdit_->setValue(25);

    auto* stepField = CreateSpinField(stepEdit_, 0.001, "0.001", parametersGroup);
    stepFieldContainer_ = stepField;
    stepEdit_->setDecimals(6);
    stepEdit_->setRange(0.000001, 0.1);
    stepEdit_->setSingleStep(0.0005);
    stepEdit_->setValue(0.001);

    auto* alphaField = CreateSpinField(alphaEdit_, 0.050, "0.050", parametersGroup);
    alphaEdit_->setDecimals(3);
    alphaEdit_->setRange(0.001, 0.250);
    alphaEdit_->setSingleStep(0.01);
    alphaEdit_->setValue(0.050);

    ApplyInputPalette(methodComboBox_);
    ApplyInputPalette(sampleSizeEdit_);
    ApplyInputPalette(binsEdit_);
    ApplyInputPalette(stepEdit_);
    ApplyInputPalette(alphaEdit_);

    parametersForm->addRow("Метод", methodComboBox_);
    parametersForm->addRow("Размер выборки n", sampleSizeField);
    parametersForm->addRow("Интервалы гистограммы", binsField);
    stepLabel_ = new QLabel("Шаг Δy", parametersGroup);
    parametersForm->addRow(stepLabel_, stepField);
    parametersForm->addRow("Уровень значимости α", alphaField);

    auto* theoryGroup = new QGroupBox("Теория варианта", controlCard);
    theoryGroup->setObjectName("theoryGroup");
    auto* theoryLayout = new QGridLayout(theoryGroup);
    theoryLayout->setContentsMargins(18, 20, 18, 18);
    theoryLayout->setHorizontalSpacing(12);
    theoryLayout->setVerticalSpacing(10);
    theoryLayout->setColumnStretch(1, 1);

    auto addTheoryRow = [theoryLayout, theoryGroup](int row,
                                                    const QString& title,
                                                    const QString& value,
                                                    bool richText = false,
                                                    QLabel** titleOut = nullptr,
                                                    QLabel** valueOut = nullptr) {
        auto* titleLabel = new QLabel(title, theoryGroup);
        titleLabel->setObjectName("theoryKey");
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        auto* valueLabel = new QLabel(value, theoryGroup);
        valueLabel->setObjectName("theoryValue");
        valueLabel->setWordWrap(true);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        if (richText)
        {
            valueLabel->setTextFormat(Qt::RichText);
        }

        theoryLayout->addWidget(titleLabel, row, 0, Qt::AlignLeft | Qt::AlignTop);
        theoryLayout->addWidget(valueLabel, row, 1);

        if (titleOut != nullptr)
        {
            *titleOut = titleLabel;
        }

        if (valueOut != nullptr)
        {
            *valueOut = valueLabel;
        }
    };

    addTheoryRow(0, "Плотность", "f(x)=2x, 0≤x≤1");
    addTheoryRow(1, "Функция распределения", "F(x)=x<sup>2</sup>, 0≤x≤1", true);
    addTheoryRow(2, "Математическое ожидание", "2/3");
    addTheoryRow(3, "Дисперсия", "1/18");
    addTheoryRow(4, "Медиана", "1/√2");
    addTheoryRow(5, "Мода", "1");
    addTheoryRow(6,
                 "Для метода исключения",
                 "a=0, b=1, M=2, критерий принятия z&lt;y",
                 true,
                 &rejectionTheoryLabel_,
                 &rejectionTheoryValueLabel_);

    runButton_ = new QPushButton("Выполнить моделирование", controlCard);
    runButton_->setObjectName("primaryButton");
    runButton_->setMinimumHeight(46);

    statusBadge_ = new QLabel(controlCard);
    statusBadge_->setWordWrap(true);

    controlLayout->addWidget(titleLabel);
    controlLayout->addWidget(parametersGroup);
    controlLayout->addWidget(theoryGroup);
    controlLayout->addWidget(runButton_);
    controlLayout->addWidget(statusBadge_);
    controlLayout->addStretch();

    UpdateMethodSpecificInputsVisibility();

    auto* resultsCard = new QWidget(splitter);
    resultsCard->setObjectName("resultsCard");

    auto* resultsLayout = new QVBoxLayout(resultsCard);
    resultsLayout->setContentsMargins(18, 18, 18, 18);
    resultsLayout->setSpacing(12);

    auto* resultsTitle = new QLabel("Результаты моделирования", resultsCard);
    resultsTitle->setObjectName("resultsTitle");

    auto* summaryGroup = new QGroupBox("Итоги расчёта", resultsCard);
    summaryGroup->setObjectName("summaryGroup");
    auto* summaryLayout = new QGridLayout(summaryGroup);
    summaryLayout->setContentsMargins(14, 18, 14, 14);
    summaryLayout->setHorizontalSpacing(18);
    summaryLayout->setVerticalSpacing(10);

    auto addSummaryRow = [summaryLayout, resultsCard](int row,
                                                      int column,
                                                      const QString& title,
                                                      QLabel*& valueLabel)
    {
        auto* caption = new QLabel(title, resultsCard);
        caption->setObjectName("summaryCaption");
        valueLabel = new QLabel("—", resultsCard);
        valueLabel->setObjectName("summaryValue");

        summaryLayout->addWidget(caption, row, column * 2);
        summaryLayout->addWidget(valueLabel, row, column * 2 + 1);
    };

    addSummaryRow(0, 0, "Размер выборки", sampleCountValue_);
    addSummaryRow(0, 1, "Среднее", meanValue_);
    addSummaryRow(1, 0, "Дисперсия", varianceValue_);
    addSummaryRow(1, 1, "D<sub>n</sub>", dnValue_);
    addSummaryRow(2, 0, "K<sub>n</sub>", knValue_);
    addSummaryRow(2, 1, "K<sub>1-α</sub>", criticalValue_);
    addSummaryRow(3, 0, "Гипотеза H<sub>0</sub>", hypothesisValue_);
    addSummaryRow(3, 1, "Доля принятия", acceptanceValue_);

    resultsTabs_ = new QTabWidget(resultsCard);
    resultsTabs_->setObjectName("resultsTabs");
    resultsTabs_->setMovable(true);

    auto* histogramTab = new QWidget(resultsTabs_);
    auto* histogramLayout = new QVBoxLayout(histogramTab);
    histogramLayout->setContentsMargins(0, 0, 0, 0);
    histogramPlot_ = new PlotChartWidget("Гистограмма и теоретическая плотность", "x", "плотность", histogramTab);
    histogramLayout->addWidget(histogramPlot_);

    auto* distributionTab = new QWidget(resultsTabs_);
    auto* distributionLayout = new QVBoxLayout(distributionTab);
    distributionLayout->setContentsMargins(0, 0, 0, 0);
    cdfPlot_ =
        new PlotChartWidget("Выборочная и теоретическая функции распределения", "x", "F(x)", distributionTab);
    distributionLayout->addWidget(cdfPlot_);

    auto* sampleTab = new QWidget(resultsTabs_);
    auto* sampleLayout = new QVBoxLayout(sampleTab);
    sampleLayout->setContentsMargins(0, 0, 0, 0);
    samplePlot_ = new PlotChartWidget("Последовательность сгенерированных значений", "номер элемента", "x", sampleTab);
    sampleLayout->addWidget(samplePlot_);

    auto* tableTab = new QWidget(resultsTabs_);
    auto* tableLayout = new QVBoxLayout(tableTab);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    auto* tableCard = new QFrame(tableTab);
    tableCard->setObjectName("plotCard");
    auto* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(12, 12, 12, 12);
    tableCardLayout->setSpacing(10);

    auto* tableTitle = new QLabel("Таблица выборки", tableCard);
    tableTitle->setObjectName("tableTitle");
    samplesTable_ = new QTableView(tableCard);
    samplesModel_ = new SampleTableModel(samplesTable_);
    samplesTable_->setModel(samplesModel_);
    samplesTable_->setAlternatingRowColors(true);
    samplesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    samplesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    samplesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    samplesTable_->setWordWrap(false);
    samplesTable_->verticalHeader()->setVisible(false);
    samplesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    samplesTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    tableCardLayout->addWidget(tableTitle);
    tableCardLayout->addWidget(samplesTable_);
    tableLayout->addWidget(tableCard);

    resultsTabs_->addTab(histogramTab, "Гистограмма");
    resultsTabs_->addTab(distributionTab, "Функция F(x)");
    resultsTabs_->addTab(sampleTab, "Выборка");
    resultsTabs_->addTab(tableTab, "Таблица");

    resultsLayout->addWidget(resultsTitle);
    resultsLayout->addWidget(summaryGroup);
    resultsLayout->addWidget(resultsTabs_, 1);

    splitter->addWidget(controlCard);
    splitter->addWidget(resultsCard);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({520, 980});

    rootLayout->addWidget(splitter);
    setCentralWidget(central);
}

void MainWindow::ApplyTheme()
{
    setStyleSheet(R"(
        QMainWindow {
            background: #eef3f8;
        }

        QWidget#controlCard, QWidget#resultsCard {
            background: #fbfdff;
            border: 1px solid #d8e2ed;
            border-radius: 18px;
        }

        QSplitter::handle {
            background: #dbe6f2;
            margin: 20px 0;
            border-radius: 2px;
        }

        QSplitter::handle:hover {
            background: #c5d5e6;
        }

        QLabel {
            color: #102033;
            font-size: 15px;
        }

        QLabel#titleLabel, QLabel#resultsTitle {
            color: #102033;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#tableTitle {
            color: #102033;
            font-size: 18px;
            font-weight: 700;
        }

        QLabel#summaryCaption {
            color: #4d647a;
            font-size: 15px;
            font-weight: 600;
        }

        QLabel#summaryValue {
            color: #102033;
            font-size: 15px;
            font-weight: 700;
        }

        QFrame#plotCard {
            border: 1px solid #d9e2ec;
            border-radius: 16px;
            background: #ffffff;
        }

        QLabel#plotTitle {
            color: #102033;
            font-size: 16px;
            font-weight: 700;
        }

        QChartView#plotChartView {
            background: transparent;
            border: none;
        }

        QGroupBox {
            color: #102033;
            font-size: 14px;
            font-weight: 700;
            border: 1px solid #d9e2ec;
            border-radius: 14px;
            margin-top: 12px;
            background: #ffffff;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }

        QGroupBox#parametersGroup,
        QGroupBox#summaryGroup,
        QGroupBox#theoryGroup {
            margin-top: 18px;
        }

        QGroupBox#parametersGroup::title,
        QGroupBox#summaryGroup::title {
            color: #17324d;
            font-size: 26px;
            font-weight: 800;
            left: 16px;
            padding: 0 10px 2px 10px;
        }

        QGroupBox#theoryGroup::title {
            color: #17324d;
            font-size: 26px;
            font-weight: 800;
            left: 16px;
            padding: 0 10px 2px 10px;
        }

        QGroupBox#parametersGroup QLabel {
            font-size: 15px;
            font-weight: 600;
            color: #20384f;
        }

        QLabel#theoryKey {
            color: #294560;
            font-size: 15px;
            font-weight: 700;
        }

        QLabel#theoryValue {
            color: #102033;
            font-size: 15px;
            font-weight: 500;
        }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            font-size: 15px;
            min-height: 36px;
            border: 1px solid #c8d4e2;
            border-radius: 10px;
            padding: 0 10px;
            background: #ffffff;
            color: #102033;
            selection-background-color: #2a6df4;
            selection-color: #ffffff;
        }

        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #2a6df4;
        }

        QPushButton#spinAdjustButton {
            background: #f7fbff;
            color: #1e3655;
            border: 1px solid #d7e2ee;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 700;
            padding: 0;
        }

        QPushButton#spinAdjustButton:hover {
            background: #edf5ff;
            border-color: #bdd0e6;
        }

        QPushButton#spinAdjustButton:pressed {
            background: #dfeaf8;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #d7e2ee;
            background: #f7fbff;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }

        QComboBox#methodComboBox {
            combobox-popup: 0;
            background: #ffffff;
            border: 1px solid #ccd9e7;
            border-radius: 10px;
            color: #1e3655;
            min-height: 40px;
            padding-top: 2px;
            padding-bottom: 2px;
            padding-right: 28px;
        }

        QComboBox#methodComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #d7e2ee;
            background: #f7fbff;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }

        QComboBox#methodComboBox::down-arrow {
            image: url(:/icons/arrow-down.svg);
            width: 12px;
            height: 12px;
        }

        QComboBox#methodComboBox[popupOpen="true"]::down-arrow {
            image: url(:/icons/arrow-up.svg);
        }

        QComboBox#methodComboBox QAbstractItemView {
            background: #ffffff;
            border: 1px solid #c8d8ea;
            outline: none;
            selection-background-color: #e6f1fb;
            selection-color: #173b5f;
            color: #1e3655;
            padding: 4px;
        }

        QComboBox#methodComboBox QAbstractItemView::item {
            min-height: 30px;
            padding: 4px 8px;
        }

        QPushButton#primaryButton {
            background: #2a6df4;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 15px;
            font-weight: 700;
            padding: 0 16px;
        }

        QPushButton#primaryButton:hover {
            background: #1f61e4;
        }

        QPushButton#primaryButton:pressed {
            background: #174fc4;
        }

        QPushButton#primaryButton:disabled {
            background: #b2c8f2;
            color: #eaf1ff;
        }

        QTabWidget#resultsTabs::pane {
            border: none;
            top: -1px;
            background: transparent;
        }

        QTabWidget#resultsTabs::tab-bar {
            left: 14px;
        }

        QTabWidget#resultsTabs QTabBar::tab {
            background: #eef3f8;
            color: #46627d;
            border: 1px solid #d8e2ed;
            border-bottom: none;
            padding: 11px 18px;
            margin-right: 6px;
            min-width: 120px;
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
            font-size: 15px;
            font-weight: 700;
        }

        QTabWidget#resultsTabs QTabBar::tab:hover {
            background: #e8f0f8;
            color: #284560;
        }

        QTabWidget#resultsTabs QTabBar::tab:selected {
            background: #ffffff;
            color: #102033;
            margin-bottom: -1px;
        }

        QTableView {
            border: 1px solid #d9e2ec;
            border-radius: 12px;
            gridline-color: #e5edf5;
            color: #102033;
            selection-background-color: #dce8ff;
            selection-color: #102033;
            alternate-background-color: #f7fbff;
            background: #ffffff;
            font-size: 15px;
        }

        QHeaderView::section {
            background: #f3f7fb;
            color: #102033;
            border: none;
            border-bottom: 1px solid #d9e2ec;
            padding: 8px;
            font-size: 15px;
            font-weight: 700;
        }

        QTableCornerButton::section {
            background: #f3f7fb;
            border: none;
            border-bottom: 1px solid #d9e2ec;
        }

        QScrollBar:vertical {
            background: #eef4fb;
            width: 12px;
            margin: 10px 2px 10px 2px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical {
            background: #c7d7e8;
            min-height: 36px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical:hover {
            background: #b4c9de;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            background: transparent;
            border: none;
        }

        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }

        QScrollBar:horizontal {
            background: #eef4fb;
            height: 12px;
            margin: 2px 10px 2px 10px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal {
            background: #c7d7e8;
            min-width: 36px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal:hover {
            background: #b4c9de;
        }

        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0px;
            background: transparent;
            border: none;
        }

        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");
}

void MainWindow::SetupSignals()
{
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::RunSampling);
    connect(methodComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        UpdateMethodSpecificInputsVisibility();
    });

    connect(simulationWatcher_, &QFutureWatcher<SamplingResult>::finished, this, [this]() {
        FinishSampling(simulationWatcher_->result());
    });
}

void MainWindow::ResetOutputs()
{
    sampleCountValue_->setText("—");
    meanValue_->setText("—");
    varianceValue_->setText("—");
    dnValue_->setText("—");
    knValue_->setText("—");
    criticalValue_->setText("—");
    hypothesisValue_->setText("—");
    acceptanceValue_->setText("—");
    hypothesisValue_->setStyleSheet("");

    histogramPlot_->Clear();
    cdfPlot_->Clear();
    samplePlot_->Clear();

    static_cast<SampleTableModel*>(samplesModel_)->SetResult(nullptr);

    hasResult_ = false;
    lastResult_ = SamplingResult{};
}

void MainWindow::RunSampling()
{
    if (isSamplingRunning_)
    {
        return;
    }

    ResetOutputs();

    QString errorMessage;
    if (!ValidateInput(errorMessage))
    {
        ShowStatus(errorMessage, false);
        return;
    }

    SamplingOptions options;
    options.method = methodComboBox_->currentIndex() == 0 ? SamplingMethod::InverseFunction
                                                          : SamplingMethod::Rejection;
    options.sampleSize = static_cast<std::size_t>(sampleSizeEdit_->value());
    options.histogramBins = static_cast<std::size_t>(binsEdit_->value());
    options.integrationStep = stepEdit_->value();
    options.alpha = alphaEdit_->value();

    ApplySamplingRunningState(true);
    ShowStatus("Идёт моделирование. После расчёта будут построены графики, таблица и статистика.", true);

    simulationWatcher_->setFuture(QtConcurrent::run([options]() { return ::RunSampling(options); }));
}

bool MainWindow::ValidateInput(QString& errorMessage) const
{
    if (sampleSizeEdit_->value() <= 0)
    {
        errorMessage = "Размер выборки должен быть больше нуля.";
        return false;
    }

    if (binsEdit_->value() <= 0)
    {
        errorMessage = "Число интервалов гистограммы должно быть больше нуля.";
        return false;
    }

    if (methodComboBox_->currentIndex() == 0 && !(stepEdit_->value() > 0.0))
    {
        errorMessage = "Шаг Δy должен быть больше нуля.";
        return false;
    }

    if (!(alphaEdit_->value() > 0.0 && alphaEdit_->value() < 1.0))
    {
        errorMessage = "Уровень значимости α должен принадлежать интервалу (0, 1).";
        return false;
    }

    return true;
}

void MainWindow::UpdateMethodSpecificInputsVisibility()
{
    const bool isInverseMethod = methodComboBox_ != nullptr && methodComboBox_->currentIndex() == 0;
    const bool showStep = isInverseMethod;
    const bool enableStep = showStep && !isSamplingRunning_;
    const bool showRejectionTheory = !isInverseMethod;

    if (stepLabel_ != nullptr)
    {
        stepLabel_->setVisible(showStep);
    }

    if (stepFieldContainer_ != nullptr)
    {
        stepFieldContainer_->setVisible(showStep);
        stepFieldContainer_->setEnabled(enableStep);
    }

    if (stepEdit_ != nullptr)
    {
        stepEdit_->setVisible(showStep);
        stepEdit_->setEnabled(enableStep);
    }

    if (rejectionTheoryLabel_ != nullptr)
    {
        rejectionTheoryLabel_->setVisible(showRejectionTheory);
    }

    if (rejectionTheoryValueLabel_ != nullptr)
    {
        rejectionTheoryValueLabel_->setVisible(showRejectionTheory);
    }
}

void MainWindow::ApplySamplingRunningState(bool running)
{
    isSamplingRunning_ = running;

    methodComboBox_->setEnabled(!running);
    SetFieldContainerEnabled(sampleSizeEdit_, !running);
    SetFieldContainerEnabled(binsEdit_, !running);
    SetFieldContainerEnabled(alphaEdit_, !running);
    UpdateMethodSpecificInputsVisibility();

    runButton_->setEnabled(!running);
    runButton_->setText(running ? "Идёт моделирование..." : "Выполнить моделирование");
}

void MainWindow::FinishSampling(const SamplingResult& result)
{
    ApplySamplingRunningState(false);

    if (!result.success)
    {
        ShowStatus(QString::fromStdString(result.message), false);
        return;
    }

    lastResult_ = result;
    hasResult_ = true;

    PopulateTable(result);
    UpdateSummary(result);
    UpdatePlots(result);

    const auto* model = static_cast<SampleTableModel*>(samplesModel_);
    QString statusText = QString::fromStdString(result.message);
    if (model != nullptr && model->IsSampled())
    {
        statusText += QString(". Для таблицы включена равномерная выборка: каждая %1-я строка.")
                          .arg(model->VisibleRowStep());
    }
    ShowStatus(statusText, true);
}

void MainWindow::PopulateTable(const SamplingResult& result)
{
    Q_UNUSED(result);
    static_cast<SampleTableModel*>(samplesModel_)->SetResult(&lastResult_, kMaxTablePreviewRows);
}

void MainWindow::UpdateSummary(const SamplingResult& result)
{
    sampleCountValue_->setText(QString::number(result.samples.size()));
    meanValue_->setText(FormatNumber(result.empiricalStatistics.mean));
    varianceValue_->setText(FormatNumber(result.empiricalStatistics.variance));
    dnValue_->setText(FormatNumber(result.kolmogorov.dn));
    knValue_->setText(FormatNumber(result.kolmogorov.kn));
    criticalValue_->setText(FormatNumber(result.kolmogorov.criticalValue));
    hypothesisValue_->setText(result.kolmogorov.rejectNullHypothesis ? "H₀ отвергается"
                                                                     : "H₀ не отвергается");
    hypothesisValue_->setStyleSheet(result.kolmogorov.rejectNullHypothesis
                                        ? "color: #9b1c1c; font-weight: 700;"
                                        : "color: #1d5a32; font-weight: 700;");

    if (result.acceptanceRate >= 0.0)
    {
        acceptanceValue_->setText(FormatNumber(result.acceptanceRate));
    }
    else
    {
        acceptanceValue_->setText("—");
    }
}

void MainWindow::UpdatePlots(const SamplingResult& result)
{
    histogramPlot_->SetSeries(
        {
            {"Гистограмма выборки", BuildHistogramOutline(result.histogram), QColor("#2563eb"), 2.4, Qt::SolidLine},
            {"Теоретическая плотность  f(x)=2x", BuildDensityCurvePoints(), QColor("#dc2626"), 2.0, Qt::DashLine},
        });

    cdfPlot_->SetSeries(
        {
            {"Эмпирическая функция  F(x)", BuildEmpiricalDistributionPoints(result.sortedSamples), QColor("#1d4ed8"), 2.4, Qt::SolidLine},
            {"Теоретическая функция  F(x)=x²", BuildTheoreticalDistributionPoints(), QColor("#059669"), 2.0, Qt::DashLine},
        });

    samplePlot_->SetSeries(
        {
            {QString("%1: x_i").arg(MethodTitle(result.method)),
             BuildSampleSequence(result.samples),
             QColor("#7c3aed"),
             1.8,
             Qt::SolidLine},
        });
}

void MainWindow::ShowStatus(const QString& text, bool success)
{
    statusBadge_->setText(text);

    if (success)
    {
        statusBadge_->setStyleSheet(
            "border: 1px solid #94d7aa; background: #eaf8ef; color: #1d5a32; "
            "border-radius: 12px; padding: 10px 12px;");
    }
    else
    {
        statusBadge_->setStyleSheet(
            "border: 1px solid #efb7bf; background: #fff1f3; color: #7b2636; "
            "border-radius: 12px; padding: 10px 12px;");
    }
}

QString MainWindow::FormatNumber(double value)
{
    return FormatNumericValue(value);
}
