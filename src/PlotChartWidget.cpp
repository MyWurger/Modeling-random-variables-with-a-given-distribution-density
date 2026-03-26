#include "PlotChartWidget.h"

#include <QGestureEvent>
#include <QFontMetricsF>
#include <QGraphicsLayout>
#include <QGraphicsView>
#include <QLabel>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPinchGesture>
#include <QPointer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QtCharts/QLegend>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <cmath>
#include <optional>

namespace
{
constexpr int kMaxRenderedPoints = 20000;
constexpr double kMinZoomFactor = 0.75;
constexpr double kMaxZoomFactor = 1.35;
constexpr double kWheelZoomStep = 1.15;
constexpr double kAxisArrowLength = 9.0;
constexpr double kAxisArrowHalfWidth = 4.5;
constexpr double kSupportLeft = 0.0;
constexpr double kSupportRight = 1.0;

double NiceTickStep(double span, int targetTicks)
{
    if (!std::isfinite(span) || span <= 0.0 || targetTicks < 2)
    {
        return 1.0;
    }

    const double rawStep = span / static_cast<double>(targetTicks - 1);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double normalized = rawStep / magnitude;

    double nice = 1.0;
    if (normalized >= 7.5)
    {
        nice = 10.0;
    }
    else if (normalized >= 3.5)
    {
        nice = 5.0;
    }
    else if (normalized >= 1.5)
    {
        nice = 2.0;
    }

    return nice * magnitude;
}

int DecimalsForStep(double step)
{
    if (!std::isfinite(step) || step <= 0.0)
    {
        return 6;
    }

    if (step >= 1.0)
    {
        return 0;
    }

    const int decimals = static_cast<int>(std::ceil(-std::log10(step))) + 1;
    return std::clamp(decimals, 0, 12);
}

double TickAnchorForRange(double minValue, double step)
{
    if (!std::isfinite(minValue) || !(step > 0.0) || !std::isfinite(step))
    {
        return 0.0;
    }

    return std::floor(minValue / step) * step;
}

double StableTickStep(double previousStep, double span, int targetTicks)
{
    Q_UNUSED(previousStep);
    const double desired = NiceTickStep(span, targetTicks);
    return desired;
}

QString FormatHoverValue(double value)
{
    if (!std::isfinite(value))
    {
        return QStringLiteral("—");
    }

    if (qFuzzyIsNull(value))
    {
        return QStringLiteral("0");
    }

    const double absoluteValue = std::abs(value);
    if (absoluteValue >= 1.0e4 || absoluteValue < 1.0e-4)
    {
        return QString::number(value, 'e', 3);
    }

    QString text = QString::number(value, 'f', 4);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.')))
    {
        text.chop(1);
    }

    if (text == "-0")
    {
        return QStringLiteral("0");
    }

    return text;
}

std::optional<double> InterpolateSeriesValueAtX(const QVector<QPointF>& points,
                                                bool monotonicByX,
                                                double xValue)
{
    if (points.isEmpty() || !std::isfinite(xValue))
    {
        return std::nullopt;
    }

    if (!monotonicByX || points.size() == 1)
    {
        const auto nearestIt = std::min_element(
            points.cbegin(),
            points.cend(),
            [xValue](const QPointF& left, const QPointF& right) {
                return std::abs(left.x() - xValue) < std::abs(right.x() - xValue);
            });

        return nearestIt != points.cend() ? std::optional<double>(nearestIt->y()) : std::nullopt;
    }

    const auto rightIt =
        std::lower_bound(points.cbegin(), points.cend(), xValue, [](const QPointF& point, double value) {
            return point.x() < value;
        });

    if (rightIt == points.cbegin())
    {
        return points.front().y();
    }

    if (rightIt == points.cend())
    {
        return points.back().y();
    }

    const QPointF rightPoint = *rightIt;
    const QPointF leftPoint = *(rightIt - 1);
    const double dx = rightPoint.x() - leftPoint.x();
    if (std::abs(dx) < 1.0e-12)
    {
        return std::max(leftPoint.y(), rightPoint.y());
    }

    const double t = std::clamp((xValue - leftPoint.x()) / dx, 0.0, 1.0);
    return leftPoint.y() + (rightPoint.y() - leftPoint.y()) * t;
}

QVector<QPointF> DownsamplePoints(const QVector<QPointF>& points)
{
    if (points.size() <= kMaxRenderedPoints)
    {
        return points;
    }

    QVector<QPointF> sampled;
    sampled.reserve(kMaxRenderedPoints);

    const double step =
        static_cast<double>(points.size() - 1) / static_cast<double>(kMaxRenderedPoints - 1);
    const int maxIndex = points.size() - 1;

    for (int i = 0; i < kMaxRenderedPoints - 1; ++i)
    {
        const int index = static_cast<int>(std::round(i * step));
        sampled.append(points[std::clamp(index, 0, maxIndex)]);
    }

    sampled.append(points.back());
    return sampled;
}

bool IsMonotonicByX(const QVector<QPointF>& points)
{
    for (int i = 1; i < points.size(); ++i)
    {
        if (points[i].x() < points[i - 1].x())
        {
            return false;
        }
    }

    return true;
}

QVector<QPointF> BuildRenderedPoints(const QVector<QPointF>& fullPoints,
                                     bool monotonicByX,
                                     double visibleXMin,
                                     double visibleXMax)
{
    if (fullPoints.size() <= kMaxRenderedPoints)
    {
        return fullPoints;
    }

    if (!monotonicByX)
    {
        return DownsamplePoints(fullPoints);
    }

    const auto leftIt = std::lower_bound(
        fullPoints.cbegin(),
        fullPoints.cend(),
        visibleXMin,
        [](const QPointF& point, double value) { return point.x() < value; });

    const auto rightIt = std::upper_bound(
        fullPoints.cbegin(),
        fullPoints.cend(),
        visibleXMax,
        [](double value, const QPointF& point) { return value < point.x(); });

    int firstIndex = static_cast<int>(std::distance(fullPoints.cbegin(), leftIt));
    int lastIndex = static_cast<int>(std::distance(fullPoints.cbegin(), rightIt)) - 1;

    firstIndex = std::max(0, firstIndex - 1);
    lastIndex = std::min(static_cast<int>(fullPoints.size()) - 1, std::max(firstIndex, lastIndex + 1));

    QVector<QPointF> visiblePoints;
    visiblePoints.reserve(lastIndex - firstIndex + 1);
    for (int index = firstIndex; index <= lastIndex; ++index)
    {
        visiblePoints.append(fullPoints[index]);
    }

    if (visiblePoints.size() <= kMaxRenderedPoints)
    {
        return visiblePoints;
    }

    return DownsamplePoints(visiblePoints);
}
} // namespace

class InteractiveChartView final : public QChartView
{
public:
    explicit InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent = nullptr)
        : QChartView(chart, parent), owner_(owner)
    {
        setDragMode(QGraphicsView::NoDrag);
        setRubberBand(QChartView::NoRubberBand);
        setMouseTracking(true);
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
        grabGesture(Qt::PinchGesture);
        viewport()->grabGesture(Qt::PinchGesture);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QChartView::paintEvent(event);

        if (owner_ == nullptr || !owner_->hasData_ || chart() == nullptr || owner_->anchorSeries_ == nullptr)
        {
            return;
        }

        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor("#4b647b"), 1.35));

        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();

        if (yMin <= 0.0 && yMax >= 0.0)
        {
            DrawHorizontalAxis(&painter, xMin, xMax);
        }

        if (xMin <= 0.0 && xMax >= 0.0)
        {
            DrawVerticalAxis(&painter, yMin, yMax);
        }

        if (owner_->xAxisLabel_ == "x")
        {
            DrawSupportBoundary(&painter, kSupportLeft);
            DrawSupportBoundary(&painter, kSupportRight);
        }

        if (hoverActive_)
        {
            DrawHoverOverlay(&painter);
        }
    }

    bool viewportEvent(QEvent* event) override
    {
        if (event == nullptr || owner_ == nullptr)
        {
            return QChartView::viewportEvent(event);
        }

        if (event->type() == QEvent::NativeGesture)
        {
            auto* nativeEvent = static_cast<QNativeGestureEvent*>(event);
            if (HandleNativeGesture(nativeEvent))
            {
                return true;
            }
        }

        if (event->type() == QEvent::Gesture)
        {
            auto* gestureEvent = static_cast<QGestureEvent*>(event);
            if (HandleGesture(gestureEvent))
            {
                return true;
            }
        }

        return QChartView::viewportEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event == nullptr || owner_ == nullptr || !owner_->hasData_)
        {
            QChartView::wheelEvent(event);
            return;
        }

        if ((event->modifiers() & Qt::ControlModifier) != 0)
        {
            QChartView::wheelEvent(event);
            return;
        }

        const QPoint pixelDelta = event->pixelDelta();
        if (!pixelDelta.isNull())
        {
            owner_->PanByPixels(pixelDelta);
            event->accept();
            return;
        }

        if (event->angleDelta().y() != 0)
        {
            const double factor =
                event->angleDelta().y() > 0 ? (1.0 / kWheelZoomStep) : kWheelZoomStep;
            owner_->ZoomAt(event->position(), factor);
            event->accept();
            return;
        }

        QChartView::wheelEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (owner_ != nullptr)
        {
            owner_->ResetView();
            event->accept();
            return;
        }

        QChartView::mouseDoubleClickEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (event != nullptr)
        {
            hoverViewportPos_ = event->position();
            hoverActive_ = IsPointInsidePlot(hoverViewportPos_);
            viewport()->update();
        }

        QChartView::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        hoverActive_ = false;
        viewport()->update();
        QChartView::leaveEvent(event);
    }

private:
    struct HoverEntry
    {
        QString name;
        QString value;
        QColor color;
        QPointF markerViewPoint;
    };

    bool IsPointInsidePlot(const QPointF& viewportPoint) const
    {
        if (chart() == nullptr)
        {
            return false;
        }

        const QPointF scenePos = mapToScene(viewportPoint.toPoint());
        const QPointF chartPos = chart()->mapFromScene(scenePos);
        return chart()->plotArea().contains(chartPos);
    }

    void DrawHoverOverlay(QPainter* painter)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr ||
            !IsPointInsidePlot(hoverViewportPos_))
        {
            return;
        }

        const QPointF scenePos = mapToScene(hoverViewportPos_.toPoint());
        const QPointF chartPos = chart()->mapFromScene(scenePos);
        const QPointF hoverValue = chart()->mapToValue(chartPos, owner_->anchorSeries_);
        const double xValue = hoverValue.x();
        if (!std::isfinite(xValue))
        {
            return;
        }

        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();
        const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->anchorSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->anchorSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        QPen guidePen(QColor(59, 130, 246, 170));
        guidePen.setWidthF(1.2);
        guidePen.setStyle(Qt::DashLine);
        painter->setPen(guidePen);
        painter->drawLine(QLineF(topViewPos, bottomViewPos));

        QVector<HoverEntry> entries;
        entries.reserve(owner_->seriesStates_.size());

        for (const PlotChartWidget::SeriesState& state : owner_->seriesStates_)
        {
            const std::optional<double> yValue =
                InterpolateSeriesValueAtX(state.fullPoints, state.monotonicByX, xValue);
            if (!yValue.has_value())
            {
                continue;
            }

            const QPointF markerChartPos = chart()->mapToPosition(QPointF(xValue, *yValue), owner_->anchorSeries_);
            const QPoint markerViewPos = mapFromScene(chart()->mapToScene(markerChartPos));
            entries.append({state.name, FormatHoverValue(*yValue), state.color, QPointF(markerViewPos)});
        }

        painter->setPen(Qt::NoPen);
        for (const HoverEntry& entry : entries)
        {
            painter->setBrush(QColor(entry.color.red(), entry.color.green(), entry.color.blue(), 235));
            painter->drawEllipse(entry.markerViewPoint, 4.5, 4.5);
        }

        DrawHoverTooltip(painter, xValue, entries);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QColor("#4b647b"));
    }

    void DrawHoverTooltip(QPainter* painter, double xValue, const QVector<HoverEntry>& entries)
    {
        if (painter == nullptr)
        {
            return;
        }

        QFont titleFont("Avenir Next");
        titleFont.setPointSize(11);
        titleFont.setWeight(QFont::DemiBold);

        QFont bodyFont("Avenir Next");
        bodyFont.setPointSize(10);
        bodyFont.setWeight(QFont::Medium);

        const QFontMetricsF titleMetrics(titleFont);
        const QFontMetricsF bodyMetrics(bodyFont);
        const QString titleText = QString("x = %1").arg(FormatHoverValue(xValue));

        constexpr qreal swatchSize = 10.0;
        constexpr qreal boxPadding = 12.0;
        constexpr qreal sectionSpacing = 8.0;
        constexpr qreal lineSpacing = 6.0;

        qreal contentWidth = titleMetrics.horizontalAdvance(titleText);
        for (const HoverEntry& entry : entries)
        {
            const QString lineText = QString("%1 = %2").arg(entry.name, entry.value);
            contentWidth = std::max(contentWidth,
                                    swatchSize + 8.0 + bodyMetrics.horizontalAdvance(lineText));
        }

        qreal contentHeight = titleMetrics.height();
        if (!entries.isEmpty())
        {
            contentHeight += sectionSpacing;
            contentHeight += entries.size() * bodyMetrics.height();
            contentHeight += (entries.size() - 1) * lineSpacing;
        }

        QRectF tooltipRect(0.0,
                           0.0,
                           contentWidth + boxPadding * 2.0,
                           contentHeight + boxPadding * 2.0);
        QPointF topLeft = hoverViewportPos_ + QPointF(18.0, -18.0);

        const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(8.0, 8.0, -8.0, -8.0);
        if (topLeft.x() + tooltipRect.width() > viewportRect.right())
        {
            topLeft.setX(hoverViewportPos_.x() - tooltipRect.width() - 18.0);
        }
        if (topLeft.x() < viewportRect.left())
        {
            topLeft.setX(viewportRect.left());
        }
        if (topLeft.y() + tooltipRect.height() > viewportRect.bottom())
        {
            topLeft.setY(viewportRect.bottom() - tooltipRect.height());
        }
        if (topLeft.y() < viewportRect.top())
        {
            topLeft.setY(viewportRect.top());
        }

        tooltipRect.moveTopLeft(topLeft);

        painter->setBrush(QColor(255, 255, 255, 242));
        painter->setPen(QPen(QColor("#d7e3ef"), 1.0));
        painter->drawRoundedRect(tooltipRect, 10.0, 10.0);

        painter->setFont(titleFont);
        painter->setPen(QColor("#102033"));
        qreal currentY = tooltipRect.top() + boxPadding + titleMetrics.ascent();
        painter->drawText(QPointF(tooltipRect.left() + boxPadding, currentY), titleText);

        if (entries.isEmpty())
        {
            return;
        }

        painter->setFont(bodyFont);
        currentY += titleMetrics.descent() + sectionSpacing;
        for (int index = 0; index < entries.size(); ++index)
        {
            const HoverEntry& entry = entries[index];
            currentY += bodyMetrics.ascent();

            const QRectF swatchRect(tooltipRect.left() + boxPadding,
                                    currentY - bodyMetrics.ascent() + 1.0,
                                    swatchSize,
                                    swatchSize);
            painter->setPen(Qt::NoPen);
            painter->setBrush(entry.color);
            painter->drawRoundedRect(swatchRect, 3.0, 3.0);

            painter->setPen(QColor("#22364d"));
            const QString lineText = QString("%1 = %2").arg(entry.name, entry.value);
            painter->drawText(QPointF(swatchRect.right() + 8.0, currentY), lineText);

            currentY += bodyMetrics.descent();
            if (index + 1 < entries.size())
            {
                currentY += lineSpacing;
            }
        }
    }

    void DrawHorizontalAxis(QPainter* painter, double xMin, double xMax)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
        {
            return;
        }

        const QPointF leftChartPos = chart()->mapToPosition(QPointF(xMin, 0.0), owner_->anchorSeries_);
        const QPointF rightChartPos = chart()->mapToPosition(QPointF(xMax, 0.0), owner_->anchorSeries_);
        const QPoint leftViewPos = mapFromScene(chart()->mapToScene(leftChartPos));
        const QPoint rightViewPos = mapFromScene(chart()->mapToScene(rightChartPos));

        DrawArrowHead(painter, QPointF(rightViewPos), QPointF(leftViewPos));
        DrawAxisLabel(
            painter,
            owner_->xAxisLabel_,
            QPointF(rightViewPos.x() - 16.0, rightViewPos.y() - 24.0),
            Qt::AlignRight | Qt::AlignBottom);
    }

    void DrawVerticalAxis(QPainter* painter, double yMin, double yMax)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
        {
            return;
        }

        const QPointF topChartPos = chart()->mapToPosition(QPointF(0.0, yMax), owner_->anchorSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(0.0, yMin), owner_->anchorSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        DrawArrowHead(painter, QPointF(topViewPos), QPointF(bottomViewPos));
        DrawAxisLabel(
            painter,
            owner_->yAxisLabel_,
            QPointF(topViewPos.x() + 16.0, topViewPos.y() - 10.0),
            Qt::AlignLeft | Qt::AlignBottom);
    }

    void DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from)
    {
        if (painter == nullptr)
        {
            return;
        }

        const QLineF direction(from, tip);
        if (qFuzzyIsNull(direction.length()))
        {
            return;
        }

        const QLineF unit = direction.unitVector();
        const QPointF delta = unit.p2() - unit.p1();
        const QPointF back = tip - delta * kAxisArrowLength;
        const QPointF normal(-delta.y(), delta.x());
        const QPointF wing1 = back + normal * kAxisArrowHalfWidth;
        const QPointF wing2 = back - normal * kAxisArrowHalfWidth;

        painter->drawLine(QLineF(tip, wing1));
        painter->drawLine(QLineF(tip, wing2));
    }

    void DrawAxisLabel(QPainter* painter, const QString& text, const QPointF& anchor, Qt::Alignment alignment)
    {
        if (painter == nullptr || text.isEmpty())
        {
            return;
        }

        QFont font = painter->font();
        font.setPointSizeF(std::max(font.pointSizeF(), 10.5));
        font.setBold(true);
        painter->setFont(font);

        const QFontMetricsF metrics(font);
        QRectF textRect = metrics.boundingRect(text);
        textRect.adjust(-6.0, -3.0, 6.0, 3.0);

        QPointF topLeft = anchor;
        if (alignment & Qt::AlignRight)
        {
            topLeft.rx() -= textRect.width();
        }
        else if (alignment & Qt::AlignHCenter)
        {
            topLeft.rx() -= textRect.width() * 0.5;
        }

        if (alignment & Qt::AlignBottom)
        {
            topLeft.ry() -= textRect.height();
        }
        else if (alignment & Qt::AlignVCenter)
        {
            topLeft.ry() -= textRect.height() * 0.5;
        }

        const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(6.0, 6.0, -6.0, -6.0);
        topLeft.setX(std::clamp(topLeft.x(), viewportRect.left(), viewportRect.right() - textRect.width()));
        topLeft.setY(std::clamp(topLeft.y(), viewportRect.top(), viewportRect.bottom() - textRect.height()));
        textRect.moveTopLeft(topLeft);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 220));
        painter->drawRoundedRect(textRect, 5.0, 5.0);
        painter->setPen(QColor("#203146"));
        painter->drawText(textRect.adjusted(6.0, 3.0, -6.0, -3.0), Qt::AlignCenter, text);
        painter->setPen(QColor("#4b647b"));
    }

    void DrawSupportBoundary(QPainter* painter, double xValue)
    {
        if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
        {
            return;
        }

        const double xMin = owner_->axisX_->min();
        const double xMax = owner_->axisX_->max();
        if (xValue < xMin || xValue > xMax)
        {
            return;
        }

        const double yMin = owner_->axisY_->min();
        const double yMax = owner_->axisY_->max();
        const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->anchorSeries_);
        const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->anchorSeries_);
        const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
        const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

        QPen boundaryPen(QColor("#94a3b8"));
        boundaryPen.setWidthF(1.2);
        boundaryPen.setStyle(Qt::DashLine);
        painter->setPen(boundaryPen);
        painter->drawLine(QLineF(topViewPos, bottomViewPos));

        DrawAxisLabel(
            painter,
            QString("x=%1").arg(xValue == 0.0 ? "0" : "1"),
            QPointF(topViewPos.x() + 8.0, topViewPos.y() + 20.0),
            Qt::AlignLeft | Qt::AlignTop);

        painter->setPen(QColor("#4b647b"));
    }

    bool HandleNativeGesture(QNativeGestureEvent* event)
    {
        if (event == nullptr || owner_ == nullptr)
        {
            return false;
        }

        const QPointF viewportPos = event->position();

        switch (event->gestureType())
        {
        case Qt::ZoomNativeGesture:
        {
            const double delta = event->value();
            if (!std::isfinite(delta) || qFuzzyIsNull(delta))
            {
                event->accept();
                return true;
            }

            const double factor = std::clamp(std::exp(-delta), kMinZoomFactor, kMaxZoomFactor);
            owner_->ZoomAt(viewportPos, factor);
            event->accept();
            return true;
        }
        case Qt::SmartZoomNativeGesture:
            owner_->ResetView();
            event->accept();
            return true;
        case Qt::PanNativeGesture:
        case Qt::SwipeNativeGesture:
            owner_->PanByPixels(event->delta());
            event->accept();
            return true;
        case Qt::BeginNativeGesture:
        case Qt::EndNativeGesture:
            event->accept();
            return true;
        default:
            return false;
        }
    }

    bool HandleGesture(QGestureEvent* event)
    {
        if (event == nullptr || owner_ == nullptr)
        {
            return false;
        }

        QGesture* gesture = event->gesture(Qt::PinchGesture);
        if (gesture == nullptr)
        {
            return false;
        }

        auto* pinch = static_cast<QPinchGesture*>(gesture);
        if ((pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) == 0)
        {
            event->accept(gesture);
            return true;
        }

        const double scale = pinch->scaleFactor();
        if (!std::isfinite(scale) || scale <= 0.0)
        {
            event->accept(gesture);
            return true;
        }

        const double factor = std::clamp(1.0 / scale, kMinZoomFactor, kMaxZoomFactor);
        owner_->ZoomAt(pinch->centerPoint(), factor);
        event->accept(gesture);
        return true;
    }

    QPointer<PlotChartWidget> owner_;
    QPointF hoverViewportPos_;
    bool hoverActive_ = false;
};

PlotChartWidget::PlotChartWidget(const QString& title,
                                 const QString& xAxisLabel,
                                 const QString& yAxisLabel,
                                 QWidget* parent)
    : QFrame(parent), xAxisLabel_(xAxisLabel), yAxisLabel_(yAxisLabel)
{
    setObjectName("plotCard");
    setMinimumHeight(220);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setObjectName("plotTitle");

    emptyStateLabel_ = new QLabel("Нет данных для отображения", this);
    emptyStateLabel_->setObjectName("plotEmptyState");
    emptyStateLabel_->setAlignment(Qt::AlignCenter);
    emptyStateLabel_->setWordWrap(true);
    emptyStateLabel_->setMinimumHeight(170);
    emptyStateLabel_->setStyleSheet(
        "QLabel#plotEmptyState {"
        " color: #5a7188;"
        " background: #f8f8fa;"
        " border: 1px dashed #c7d7e8;"
        " border-radius: 16px;"
        " font-size: 18px;"
        " font-weight: 600;"
        " padding: 24px;"
        "}");

    chart_ = new QChart();
    chart_->setTheme(QChart::ChartThemeLight);
    chart_->setBackgroundBrush(QBrush(Qt::white));
    chart_->setBackgroundRoundness(0.0);
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setPlotAreaBackgroundBrush(QColor(248, 248, 250));
    chart_->setAnimationOptions(QChart::NoAnimation);
    chart_->legend()->setVisible(false);
    chart_->legend()->setAlignment(Qt::AlignBottom);
    chart_->legend()->setLabelColor(QColor("#334155"));
    QFont legendFont("Avenir Next");
    legendFont.setPointSize(13);
    legendFont.setWeight(QFont::DemiBold);
    chart_->legend()->setFont(legendFont);
    chart_->setMargins(QMargins(0, 0, 0, 0));

    if (chart_->layout() != nullptr)
    {
        chart_->layout()->setContentsMargins(0, 0, 0, 0);
    }

    axisX_ = new QValueAxis();
    axisY_ = new QValueAxis();
    axisX_->setTitleText("");
    axisY_->setTitleText("");
    axisX_->setLabelsColor(QColor("#334155"));
    axisY_->setLabelsColor(QColor("#334155"));
    axisX_->setGridLineColor(QColor("#d3dee9"));
    axisY_->setGridLineColor(QColor("#d3dee9"));
    axisX_->setMinorGridLineColor(QColor("#e6edf5"));
    axisY_->setMinorGridLineColor(QColor("#e6edf5"));
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(0.0, 1.0);

    chart_->addAxis(axisX_, Qt::AlignBottom);
    chart_->addAxis(axisY_, Qt::AlignLeft);

    axisXSeries_ = new QLineSeries();
    axisYSeries_ = new QLineSeries();
    QPen axisPen(QColor("#4b647b"));
    axisPen.setWidthF(1.35);
    axisXSeries_->setPen(axisPen);
    axisYSeries_->setPen(axisPen);
    chart_->addSeries(axisXSeries_);
    chart_->addSeries(axisYSeries_);
    axisXSeries_->attachAxis(axisX_);
    axisXSeries_->attachAxis(axisY_);
    axisYSeries_->attachAxis(axisX_);
    axisYSeries_->attachAxis(axisY_);

    for (QLegendMarker* marker : chart_->legend()->markers(axisXSeries_))
    {
        marker->setVisible(false);
    }

    for (QLegendMarker* marker : chart_->legend()->markers(axisYSeries_))
    {
        marker->setVisible(false);
    }

    chartView_ = new InteractiveChartView(this, chart_, this);
    chartView_->setObjectName("plotChartView");
    chartView_->setRenderHint(QPainter::Antialiasing, false);
    chartView_->setMinimumHeight(170);
    chartView_->setFrameShape(QFrame::NoFrame);
    chartView_->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(titleLabel_);
    layout->addWidget(emptyStateLabel_, 1);
    layout->addWidget(chartView_, 1);

    chartView_->hide();
    RefreshAxes();
}

void PlotChartWidget::Clear()
{
    for (SeriesState& state : seriesStates_)
    {
        if (state.renderedSeries != nullptr)
        {
            chart_->removeSeries(state.renderedSeries);
            delete state.renderedSeries;
            state.renderedSeries = nullptr;
        }
    }

    seriesStates_.clear();
    anchorSeries_ = nullptr;
    chart_->legend()->setVisible(false);
    chartView_->hide();
    emptyStateLabel_->show();
    hasData_ = false;

    defaultXMin_ = 0.0;
    defaultXMax_ = 1.0;
    defaultYMin_ = 0.0;
    defaultYMax_ = 1.0;
    xTickStep_ = -1.0;
    yTickStep_ = -1.0;
    axisX_->setRange(0.0, 1.0);
    axisY_->setRange(0.0, 1.0);
    if (axisXSeries_ != nullptr)
    {
        axisXSeries_->clear();
    }
    if (axisYSeries_ != nullptr)
    {
        axisYSeries_->clear();
    }
    RefreshAxes();
}

void PlotChartWidget::SetSeries(const QVector<PlotSeriesData>& series)
{
    Clear();

    if (series.isEmpty())
    {
        return;
    }

    double xMin = 0.0;
    double xMax = 0.0;
    double yMin = 0.0;
    double yMax = 0.0;
    bool hasPoints = false;

    for (const PlotSeriesData& seriesData : series)
    {
        if (seriesData.points.isEmpty())
        {
            continue;
        }

        SeriesState state;
        state.name = seriesData.name;
        state.fullPoints = seriesData.points;
        state.color = seriesData.color;
        state.width = seriesData.width;
        state.penStyle = seriesData.penStyle;
        state.monotonicByX = IsMonotonicByX(seriesData.points);
        state.renderedSeries = new QLineSeries();
        state.renderedSeries->setName(state.name);

        QPen pen(state.color);
        pen.setWidthF(state.width);
        pen.setStyle(state.penStyle);
        state.renderedSeries->setPen(pen);

        chart_->addSeries(state.renderedSeries);
        state.renderedSeries->attachAxis(axisX_);
        state.renderedSeries->attachAxis(axisY_);

        if (anchorSeries_ == nullptr)
        {
            anchorSeries_ = state.renderedSeries;
        }

        for (const QPointF& point : state.fullPoints)
        {
            if (!hasPoints)
            {
                xMin = xMax = point.x();
                yMin = yMax = point.y();
                hasPoints = true;
            }
            else
            {
                xMin = std::min(xMin, point.x());
                xMax = std::max(xMax, point.x());
                yMin = std::min(yMin, point.y());
                yMax = std::max(yMax, point.y());
            }
        }

        seriesStates_.append(state);
    }

    if (!hasPoints || seriesStates_.isEmpty())
    {
        Clear();
        return;
    }

    chart_->legend()->setVisible(seriesStates_.size() > 1);
    chart_->legend()->setAlignment(Qt::AlignBottom);

    double xSpan = xMax - xMin;
    double ySpan = yMax - yMin;

    if (!(xSpan > 0.0) || !std::isfinite(xSpan))
    {
        const double pad = std::max(1.0, std::fabs(xMin) * 0.05);
        xMin -= pad;
        xMax += pad;
        xSpan = xMax - xMin;
    }

    if (!(ySpan > 0.0) || !std::isfinite(ySpan))
    {
        const double pad = std::max(1.0, std::fabs(yMin) * 0.05);
        yMin -= pad;
        yMax += pad;
        ySpan = yMax - yMin;
    }

    const double xPad = std::max(1e-9, xSpan * 0.10);
    const double yPad = std::max(1e-9, ySpan * 0.12);

    defaultXMin_ = xMin - xPad;
    defaultXMax_ = xMax + xPad;
    defaultYMin_ = yMin - yPad;
    defaultYMax_ = yMax + yPad;

    hasData_ = true;
    xTickStep_ = -1.0;
    yTickStep_ = -1.0;
    ApplyAxisRange(defaultXMin_, defaultXMax_, defaultYMin_, defaultYMax_);

    emptyStateLabel_->hide();
    chartView_->show();
}

void PlotChartWidget::ResetView()
{
    if (!hasData_)
    {
        Clear();
        return;
    }

    xTickStep_ = -1.0;
    yTickStep_ = -1.0;
    ApplyAxisRange(defaultXMin_, defaultXMax_, defaultYMin_, defaultYMax_);
}

void PlotChartWidget::ApplyAxisRange(double xMin, double xMax, double yMin, double yMax)
{
    if (!(xMax > xMin) || !std::isfinite(xMin) || !std::isfinite(xMax))
    {
        xMin = 0.0;
        xMax = 1.0;
    }

    if (!(yMax > yMin) || !std::isfinite(yMin) || !std::isfinite(yMax))
    {
        yMin = 0.0;
        yMax = 1.0;
    }

    const double xSpan = std::fabs(xMax - xMin);
    const double ySpan = std::fabs(yMax - yMin);

    double newXTickStep = StableTickStep(xTickStep_, xSpan, 9);
    double newYTickStep = StableTickStep(yTickStep_, ySpan, 9);

    if (!(newXTickStep > 0.0) || !std::isfinite(newXTickStep))
    {
        newXTickStep = 1.0;
    }
    if (!(newYTickStep > 0.0) || !std::isfinite(newYTickStep))
    {
        newYTickStep = 1.0;
    }

    const int xDecimals = DecimalsForStep(newXTickStep);
    const int yDecimals = DecimalsForStep(newYTickStep);

    axisX_->setTickType(QValueAxis::TicksDynamic);
    axisY_->setTickType(QValueAxis::TicksDynamic);
    axisX_->setTickAnchor(TickAnchorForRange(xMin, newXTickStep));
    axisY_->setTickAnchor(TickAnchorForRange(yMin, newYTickStep));
    axisX_->setTickInterval(newXTickStep);
    axisY_->setTickInterval(newYTickStep);
    axisX_->setMinorTickCount(3);
    axisY_->setMinorTickCount(3);
    axisX_->setLabelsAngle(0);
    axisY_->setLabelsAngle(0);
    axisX_->setLabelFormat(QString::asprintf("%%.%df", xDecimals));
    axisY_->setLabelFormat(QString::asprintf("%%.%df", yDecimals));
    axisX_->setTruncateLabels(false);
    axisY_->setTruncateLabels(false);

    xTickStep_ = newXTickStep;
    yTickStep_ = newYTickStep;

    axisX_->setRange(xMin, xMax);
    axisY_->setRange(yMin, yMax);
    UpdateAxisSeries();
    RefreshSeries();
}

void PlotChartWidget::RefreshAxes()
{
    ApplyAxisRange(axisX_->min(), axisX_->max(), axisY_->min(), axisY_->max());
}

void PlotChartWidget::UpdateAxisSeries()
{
    if (axisXSeries_ == nullptr || axisYSeries_ == nullptr)
    {
        return;
    }

    axisXSeries_->clear();
    axisYSeries_->clear();

    const double xMin = axisX_->min();
    const double xMax = axisX_->max();
    const double yMin = axisY_->min();
    const double yMax = axisY_->max();

    if (yMin <= 0.0 && yMax >= 0.0)
    {
        axisXSeries_->append(xMin, 0.0);
        axisXSeries_->append(xMax, 0.0);
    }

    if (xMin <= 0.0 && xMax >= 0.0)
    {
        axisYSeries_->append(0.0, yMin);
        axisYSeries_->append(0.0, yMax);
    }
}

void PlotChartWidget::RefreshSeries()
{
    const double visibleXMin = axisX_->min();
    const double visibleXMax = axisX_->max();

    for (SeriesState& state : seriesStates_)
    {
        if (state.renderedSeries == nullptr)
        {
            continue;
        }

        state.renderedSeries->replace(
            BuildRenderedPoints(state.fullPoints, state.monotonicByX, visibleXMin, visibleXMax));
    }
}

void PlotChartWidget::PanByPixels(const QPointF& pixelDelta)
{
    if (!hasData_ || chart_ == nullptr || !(std::isfinite(pixelDelta.x()) && std::isfinite(pixelDelta.y())))
    {
        return;
    }

    const QRectF plotArea = chart_->plotArea();
    if (!(plotArea.width() > 1.0) || !(plotArea.height() > 1.0))
    {
        return;
    }

    const double currentXMin = axisX_->min();
    const double currentXMax = axisX_->max();
    const double currentYMin = axisY_->min();
    const double currentYMax = axisY_->max();
    const double currentXSpan = currentXMax - currentXMin;
    const double currentYSpan = currentYMax - currentYMin;

    if (!(currentXSpan > 0.0) || !(currentYSpan > 0.0))
    {
        return;
    }

    const double dx = -(pixelDelta.x() / plotArea.width()) * currentXSpan;
    const double dy = (pixelDelta.y() / plotArea.height()) * currentYSpan;
    ApplyAxisRange(currentXMin + dx, currentXMax + dx, currentYMin + dy, currentYMax + dy);
}

void PlotChartWidget::ZoomAt(const QPointF& viewportPos, double factor)
{
    if (!hasData_ || !(factor > 0.0) || !std::isfinite(factor) || chartView_ == nullptr ||
        chart_ == nullptr || anchorSeries_ == nullptr)
    {
        return;
    }

    const QPointF scenePos = chartView_->mapToScene(viewportPos.toPoint());
    const QPointF chartPos = chart_->mapFromScene(scenePos);
    const QRectF plotArea = chart_->plotArea();
    if (!plotArea.contains(chartPos))
    {
        return;
    }

    const QPointF anchor = chart_->mapToValue(chartPos, anchorSeries_);
    const double currentXMin = axisX_->min();
    const double currentXMax = axisX_->max();
    const double currentYMin = axisY_->min();
    const double currentYMax = axisY_->max();
    const double currentXSpan = currentXMax - currentXMin;
    const double currentYSpan = currentYMax - currentYMin;

    if (!(currentXSpan > 0.0) || !(currentYSpan > 0.0))
    {
        return;
    }

    const double baseXSpan = std::max(1e-12, std::fabs(defaultXMax_ - defaultXMin_));
    const double baseYSpan = std::max(1e-12, std::fabs(defaultYMax_ - defaultYMin_));
    const double minXSpan = baseXSpan * 1e-6;
    const double minYSpan = baseYSpan * 1e-6;
    const double maxXSpan = baseXSpan * 1000.0;
    const double maxYSpan = baseYSpan * 1000.0;

    const double newXSpan = std::clamp(currentXSpan * factor, minXSpan, maxXSpan);
    const double newYSpan = std::clamp(currentYSpan * factor, minYSpan, maxYSpan);

    if (!(newXSpan > 0.0) || !(newYSpan > 0.0))
    {
        return;
    }

    const double xRatio = (anchor.x() - currentXMin) / currentXSpan;
    const double yRatio = (anchor.y() - currentYMin) / currentYSpan;
    const double clampedXRatio = std::clamp(xRatio, 0.0, 1.0);
    const double clampedYRatio = std::clamp(yRatio, 0.0, 1.0);

    const double newXMin = anchor.x() - newXSpan * clampedXRatio;
    const double newXMax = newXMin + newXSpan;
    const double newYMin = anchor.y() - newYSpan * clampedYRatio;
    const double newYMax = newYMin + newYSpan;

    ApplyAxisRange(newXMin, newXMax, newYMin, newYMax);
}
