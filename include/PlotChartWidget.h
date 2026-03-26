#pragma once

#include <QColor>
#include <QFrame>
#include <QPointF>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QLabel;
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
QT_END_NAMESPACE

struct PlotSeriesData
{
    QString name;
    QVector<QPointF> points;
    QColor color;
    qreal width = 2.0;
    Qt::PenStyle penStyle = Qt::SolidLine;
};

class InteractiveChartView;

class PlotChartWidget final : public QFrame
{
public:
    explicit PlotChartWidget(const QString& title,
                             const QString& xAxisLabel,
                             const QString& yAxisLabel,
                             QWidget* parent = nullptr);

    void Clear();
    void SetSeries(const QVector<PlotSeriesData>& series);

private:
    friend class InteractiveChartView;

    void ResetView();
    void ApplyAxisRange(double xMin, double xMax, double yMin, double yMax);
    void RefreshAxes();
    void RefreshSeries();
    void UpdateAxisSeries();
    void PanByPixels(const QPointF& pixelDelta);
    void ZoomAt(const QPointF& viewportPos, double factor);

    struct SeriesState
    {
        QString name;
        QVector<QPointF> fullPoints;
        QColor color;
        qreal width = 2.0;
        Qt::PenStyle penStyle = Qt::SolidLine;
        bool monotonicByX = false;
        QLineSeries* renderedSeries = nullptr;
    };

private:
    QLabel* titleLabel_ = nullptr;
    QLabel* emptyStateLabel_ = nullptr;
    QChart* chart_ = nullptr;
    QChartView* chartView_ = nullptr;
    QValueAxis* axisX_ = nullptr;
    QValueAxis* axisY_ = nullptr;
    QLineSeries* axisXSeries_ = nullptr;
    QLineSeries* axisYSeries_ = nullptr;
    QVector<SeriesState> seriesStates_;
    QLineSeries* anchorSeries_ = nullptr;
    QString xAxisLabel_;
    QString yAxisLabel_;
    double defaultXMin_ = 0.0;
    double defaultXMax_ = 1.0;
    double defaultYMin_ = 0.0;
    double defaultYMax_ = 1.0;
    double xTickStep_ = -1.0;
    double yTickStep_ = -1.0;
    bool hasData_ = false;
};
