// ============================================================================
// ФАЙЛ MAINWINDOW.CPP - РЕАЛИЗАЦИЯ ГЛАВНОГО ОКНА ПРИЛОЖЕНИЯ LAB_1
// ============================================================================
// Назначение файла:
// 1) построить интерфейс приложения;
// 2) связать поля ввода с вычислительным ядром;
// 3) отобразить результаты моделирования в виде таблицы, сводки и графиков;
// 4) управлять состоянием интерфейса во время фонового расчета.
//
// Внутри файла находятся:
// - локальные вспомогательные классы для аккуратного ввода параметров;
// - модель таблицы выборки;
// - логика построения главного окна;
// - логика обновления GUI после вычислений.
// ============================================================================

// Подключаем объявление класса MainWindow.
#include "MainWindow.h"
// Подключаем вычислительное ядро приложения.
#include "SamplingEngine.h"
// Подключаем теоретическое распределение варианта.
#include "VariantDistribution.h"

// Базовый класс абстрактной модели таблицы Qt.
#include <QAbstractTableModel>
// Выпадающий список выбора метода.
#include <QComboBox>
// Поле вещественного ввода.
#include <QDoubleSpinBox>
// Событие потери/получения фокуса.
#include <QFocusEvent>
// Форм-компоновка для пар "подпись - поле".
#include <QFormLayout>
// Наблюдатель завершения фонового вычисления.
#include <QFutureWatcher>
// Сеточная компоновка для компактных блоков данных.
#include <QGridLayout>
// Групповой контейнер с заголовком.
#include <QGroupBox>
// Управление заголовками таблицы.
#include <QHeaderView>
// Горизонтальная компоновка.
#include <QHBoxLayout>
// Текстовая подпись Qt.
#include <QLabel>
// Встроенное текстовое поле, используемое внутри spinbox.
#include <QLineEdit>
// Палитра цветов Qt-виджета.
#include <QPalette>
// Кнопка интерфейса.
#include <QPushButton>
// Список строк Qt.
#include <QStringList>
// Целочисленное поле ввода.
#include <QSpinBox>
// Разделитель левой и правой панелей.
#include <QSplitter>
// Доступ к стилю Qt-виджета.
#include <QStyle>
// Контейнер вкладок с результатами.
#include <QTabWidget>
// Табличное представление выборки.
#include <QTableView>
// Базовый класс проверки корректности текста.
#include <QValidator>
// Вертикальная компоновка.
#include <QVBoxLayout>

// QtConcurrent нужен для запуска вычислений в фоновом потоке,
// чтобы интерфейс не зависал во время моделирования.
#include <QtConcurrent/QtConcurrentRun>

// std::min, std::max, std::sort.
#include <algorithm>
// std::isfinite, std::abs.
#include <cmath>
// std::numeric_limits.
#include <limits>
// std::invalid_argument.
#include <stdexcept>

// Анонимное пространство имен хранит локальные сущности,
// которые используются только внутри этого файла.
namespace
{
// Ограничение числа шагов для эмпирической функции на графике.
constexpr std::size_t kMaxEmpiricalPlotSteps = 12000;
// Размер порции строк, которую таблица догружает за один раз.
constexpr int kTableFetchChunkSize = 2000;
// Практический верхний предел числа интервалов гистограммы.
constexpr int kMaxRecommendedHistogramBins = 2000;
// Практический верхний предел размера выборки в GUI.
constexpr int kMaxRecommendedSampleSize = 5000000;

// Локальная функция форматирования чисел для подписей и таблиц.
QString FormatNumericValue(double value);

// ----------------------------------------------------------------------------
// КЛАСС MethodComboBox - COMBOBOX С КОРРЕКТНЫМ СОСТОЯНИЕМ РАСКРЫТИЯ
// ----------------------------------------------------------------------------
// Этот класс нужен только для визуального оформления:
// он запоминает, открыт ли выпадающий список в данный момент,
// чтобы можно было поменять стрелку и стиль popup через QSS.
//
// Иначе говоря, это не "новый логический виджет",
// а небольшая обертка над обычным QComboBox
// ради более аккуратного поведения интерфейса.
// ----------------------------------------------------------------------------
class MethodComboBox final : public QComboBox
{
public:
    // Конструктор сохраняет начальное состояние "список закрыт".
    //
    // Принимает:
    // - parent: родительский Qt-виджет.
    //
    // Возвращает:
    // - созданный объект MethodComboBox.
    explicit MethodComboBox(QWidget* parent = nullptr) : QComboBox(parent)
    {
        // Начинаем с состояния "popup закрыт",
        // чтобы QSS сразу выбрал правильный вариант стрелки.
        setProperty("popupOpen", false);
    }

protected:
    // При открытии списка меняем пользовательское свойство popupOpen,
    // затем принудительно переобновляем стиль виджета.
    //
    // Это нужно потому, что QSS-стиль сам по себе не узнает,
    // открыт popup или закрыт.
    // Мы вручную переключаем свойство, чтобы:
    // - поменять стрелку вниз на стрелку вверх;
    // - при необходимости скорректировать внешний вид комбобокса.
    //
    // Принимает:
    // - ничего.
    //
    // Возвращает:
    // - ничего.
    void showPopup() override
    {
        // Помечаем, что popup сейчас будет открыт.
        setProperty("popupOpen", true);
        // unpolish/polish заставляют Qt заново применить таблицу стилей
        // с учетом нового значения свойства popupOpen.
        style()->unpolish(this);
        style()->polish(this);
        // Принудительно перерисовываем комбобокс
        // до открытия стандартного выпадающего списка.
        update();
        // После обновления собственного состояния открываем стандартный popup.
        QComboBox::showPopup();
    }

    // При закрытии popup возвращаем свойство обратно
    // и заново применяем стиль виджета.
    //
    // Принимает:
    // - ничего.
    //
    // Возвращает:
    // - ничего.
    void hidePopup() override
    {
        // Сначала закрываем стандартный popup QComboBox.
        QComboBox::hidePopup();
        // Затем помечаем состояние как "список закрыт".
        setProperty("popupOpen", false);
        // И снова обновляем стиль виджета,
        // чтобы стрелка и оформление вернулись в исходное состояние.
        style()->unpolish(this);
        style()->polish(this);
        // Обновляем виджет, чтобы он сразу отрисовался в новом состоянии.
        update();
    }
};

// ----------------------------------------------------------------------------
// КЛАСС IntegerSpinBox - УДОБНОЕ ЦЕЛОЧИСЛЕННОЕ ПОЛЕ ВВОДА
// ----------------------------------------------------------------------------
// Класс убирает стандартные кнопки QSpinBox и добавляет более аккуратное
// поведение для пустого значения, placeholder и внешних кнопок +/-.
//
// Он нужен потому, что стандартный QSpinBox:
// - не очень удобно работает с "пустым" состоянием;
// - не всегда красиво ведет себя при кастомном оформлении;
// - в нашем интерфейсе должен работать вместе с отдельными кнопками +/-.
// ----------------------------------------------------------------------------
class IntegerSpinBox final : public QSpinBox
{
public:
    // Конструктор задает базовые ограничения и поведение поля ввода.
    //
    // Принимает:
    // - parent: родительский Qt-виджет.
    //
    // Возвращает:
    // - созданный объект IntegerSpinBox.
    explicit IntegerSpinBox(QWidget* parent = nullptr) : QSpinBox(parent)
    {
        // Размер выборки и число интервалов по смыслу должны быть положительными,
        // поэтому минимальное значение здесь равно 1.
        setRange(1, 1000000);
        // Убираем встроенные кнопки spinbox,
        // потому что в интерфейсе используются внешние компактные кнопки +/-.
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        // Разрешаем ускоренное изменение значения при длительном удержании шага.
        setAccelerated(true);
        // Если пользователь ввел неточное значение,
        // spinbox подправит его к ближайшему допустимому.
        setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        // Не показываем разделители тысяч,
        // чтобы ввод выглядел компактнее и единообразнее.
        setGroupSeparatorShown(false);
    }

    // Передает placeholder во внутренний QLineEdit spinbox.
    //
    // Принимает:
    // - text: текст-подсказку, который будет виден при пустом поле.
    //
    // Возвращает:
    // - ничего.
    void SetPlaceholderText(const QString& text)
    {
        // Внутри QSpinBox реальный ввод выполняет встроенный QLineEdit,
        // поэтому placeholder ставится именно ему.
        if (auto* edit = lineEdit())
        {
            edit->setPlaceholderText(text);
        }
    }

    // Проверяет, ввел ли пользователь какое-то непустое значение вручную.
    //
    // Принимает:
    // - ничего.
    //
    // Возвращает:
    // - true, если текст поля не пустой;
    // - false, если поле сейчас пустое.
    bool HasInput() const
    {
        // Если lineEdit отсутствует, считаем поле пустым.
        const auto* edit = lineEdit();
        // Обрезаем пробелы по краям и проверяем, остался ли непустой текст.
        return edit != nullptr && !edit->text().trimmed().isEmpty();
    }

protected:
    // Если поле было пустым и пользователь просто ушел с него,
    // сохраняем состояние пустоты, а не подставляем случайное число.
    //
    // Стандартные spinbox-поля часто стремятся оставить внутри себя
    // какое-то валидное значение.
    // Нам же важно, чтобы пустое поле визуально оставалось пустым,
    // если пользователь ничего не ввел.
    //
    // Принимает:
    // - event: событие потери фокуса.
    //
    // Возвращает:
    // - ничего.
    void focusOutEvent(QFocusEvent* event) override
    {
        // Запоминаем, было ли поле пустым до стандартной обработки Qt.
        const bool wasEmpty = !HasInput();
        // Передаем событие базовому классу,
        // чтобы spinbox выполнил свою стандартную логику.
        QSpinBox::focusOutEvent(event);
        if (wasEmpty)
        {
            // Если пользователь так ничего и не ввел,
            // очищаем поле явно, сохраняя пустое состояние.
            clear();
        }
    }

    // Если пользователь нажал шаг +/- при пустом поле,
    // сначала ставим минимальное допустимое значение, а потом уже двигаемся.
    //
    // Это делает поведение поля более предсказуемым:
    // шаг вверх/вниз всегда начинается из корректной точки диапазона.
    //
    // Принимает:
    // - steps: на сколько шагов нужно изменить значение.
    //
    // Возвращает:
    // - ничего.
    void stepBy(int steps) override
    {
        if (!HasInput())
        {
            // Пустое поле нельзя "шагать" относительно пустоты,
            // поэтому сначала устанавливаем нижнюю границу диапазона.
            setValue(minimum());
        }
        // После этого выполняем обычный шаг QSpinBox.
        QSpinBox::stepBy(steps);
    }
};

// ----------------------------------------------------------------------------
// КЛАСС DecimalSpinBox - УДОБНОЕ ВЕЩЕСТВЕННОЕ ПОЛЕ ВВОДА
// ----------------------------------------------------------------------------
// Это аналог IntegerSpinBox для вещественных значений.
// Дополнительно он:
// - форматирует числа компактно;
// - понимает и точку, и запятую;
// - корректно ведет себя при пустом вводе.
//
// Такой виджет нужен для параметров вроде Δy и α,
// где пользователь вводит именно вещественные значения.
// ----------------------------------------------------------------------------
class DecimalSpinBox final : public QDoubleSpinBox
{
public:
    // Конструктор задает высокую точность и широкий допустимый диапазон.
    //
    // Принимает:
    // - parent: родительский Qt-виджет.
    //
    // Возвращает:
    // - созданный объект DecimalSpinBox.
    explicit DecimalSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent)
    {
        // Разрешаем до 6 знаков после запятой,
        // чего достаточно для шага интегрирования и похожих параметров.
        setDecimals(6);
        // Начальный шаг изменения значения.
        setSingleStep(1.0);
        // Диапазон делаем очень широким, чтобы сам виджет не ограничивал
        // форматирование и промежуточную работу с числом.
        setRange(-1.0e15, 1.0e15);
        // Убираем встроенные кнопки, так как используются внешние +/-.
        setButtonSymbols(QAbstractSpinBox::NoButtons);
        // Разрешаем ускорение при долгом удержании шага.
        setAccelerated(true);
        // При потере фокуса некорректный ввод корректируется
        // к ближайшему допустимому значению.
        setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
        // Не показываем групповые разделители, чтобы вид был единообразным.
        setGroupSeparatorShown(false);
    }

    // Передает placeholder во внутренний QLineEdit spinbox.
    //
    // Принимает:
    // - text: текст-подсказку для пустого поля.
    //
    // Возвращает:
    // - ничего.
    void SetPlaceholderText(const QString& text)
    {
        // Визуальный ввод выполняется внутренним lineEdit,
        // поэтому placeholder передается именно ему.
        if (auto* edit = lineEdit())
        {
            edit->setPlaceholderText(text);
        }
    }

    // Проверяет, есть ли у поля непустой пользовательский ввод.
    //
    // Принимает:
    // - ничего.
    //
    // Возвращает:
    // - true, если текст поля не пустой;
    // - false, если поле пустое.
    bool HasInput() const
    {
        // Если внутренний lineEdit отсутствует, считаем поле пустым.
        const auto* edit = lineEdit();
        // Обрезаем пробелы и проверяем, остался ли значимый текст.
        return edit != nullptr && !edit->text().trimmed().isEmpty();
    }

protected:
    // Форматирует отображаемое число через общую локальную функцию,
    // чтобы все числовые поля выглядели единообразно.
    //
    // Принимает:
    // - value: число, которое должно быть показано в поле.
    //
    // Возвращает:
    // - строку, которую увидит пользователь.
    QString textFromValue(double value) const override
    {
        return FormatNumericValue(value);
    }

    // Преобразует введенный пользователем текст в число.
    // Поддерживаем и точку, и запятую как десятичный разделитель.
    //
    // Принимает:
    // - text: строку, введенную пользователем.
    //
    // Возвращает:
    // - вещественное значение, полученное из текста.
    double valueFromText(const QString& text) const override
    {
        // Убираем лишние пробелы по краям.
        QString normalized = text.trimmed();
        // Приводим запятую к точке,
        // чтобы пользователь мог вводить число в привычном формате.
        normalized.replace(',', '.');

        bool ok = false;
        // Пытаемся преобразовать строку к double.
        const double value = normalized.toDouble(&ok);
        // Если преобразование не удалось, временно возвращаем 0.0.
        return ok ? value : 0.0;
    }

    // Выполняет "мягкую" валидацию текста во время набора,
    // не запрещая промежуточные состояния вроде "-" или ",".
    //
    // Принимает:
    // - text: текущий текст поля;
    // - pos: позицию курсора, которую здесь не используем.
    //
    // Возвращает:
    // - состояние валидатора Qt: Acceptable, Intermediate или Invalid.
    QValidator::State validate(QString& text, int& pos) const override
    {
        Q_UNUSED(pos);
        // Сначала анализируем текст без пробелов по краям.
        QString normalized = text.trimmed();

        // Разрешаем промежуточные состояния ввода,
        // когда пользователь еще только набирает число.
        if (normalized.isEmpty() || normalized == "-" || normalized == "+" || normalized == "." || normalized == ",")
        {
            return QValidator::Intermediate;
        }

        // Снова приводим запятую к точке перед попыткой разбора.
        normalized.replace(',', '.');
        bool ok = false;
        // Если строка уже разбирается как число, ввод допустим.
        normalized.toDouble(&ok);
        return ok ? QValidator::Acceptable : QValidator::Invalid;
    }

    // Сохраняем пустое состояние, если пользователь не ввел число.
    //
    // Принимает:
    // - event: событие потери фокуса.
    //
    // Возвращает:
    // - ничего.
    void focusOutEvent(QFocusEvent* event) override
    {
        // Запоминаем, было ли поле пустым до стандартной обработки Qt.
        const bool wasEmpty = !HasInput();
        // Выполняем обычную обработку потери фокуса базовым классом.
        QDoubleSpinBox::focusOutEvent(event);
        if (wasEmpty)
        {
            // Если пользователь так и не ввел число,
            // оставляем поле пустым явно.
            clear();
        }
    }

    // При первом шаге из пустого состояния стартуем с нуля.
    //
    // Принимает:
    // - steps: число шагов изменения значения.
    //
    // Возвращает:
    // - ничего.
    void stepBy(int steps) override
    {
        if (!HasInput())
        {
            // Переход от пустого поля начинаем с 0.0,
            // потому что для вещественных параметров это естественная база.
            setValue(0.0);
        }
        // Затем выполняем стандартный шаг QDoubleSpinBox.
        QDoubleSpinBox::stepBy(steps);
    }
};

// ----------------------------------------------------------------------------
// ФУНКЦИЯ FormatNumericValue - ЕДИНОЕ ФОРМАТИРОВАНИЕ ЧИСЕЛ ДЛЯ GUI
// ----------------------------------------------------------------------------
// Эта функция нужна, чтобы:
// - не показывать лишние нули;
// - компактно оформлять очень большие и очень маленькие значения;
// - одинаково выводить числа в полях, таблице и подписях.
//
// Принимает:
// - value: вещественное число, которое нужно преобразовать в строку.
//
// Возвращает:
// - строковое представление числа в удобном для GUI виде.
// ----------------------------------------------------------------------------
QString FormatNumericValue(double value)
{
    // Невалидные вещественные значения показываем явно как "не число".
    if (!std::isfinite(value))
    {
        return "не число";
    }

    // Ноль выводим отдельно, чтобы избежать вариантов вроде 0.000000.
    if (value == 0.0)
    {
        return "0";
    }

    const double absoluteValue = std::abs(value);
    // Для очень маленьких и очень больших величин используем экспоненциальную запись.
    // Это позволяет не получать длинные неудобные строки
    // вроде 0.000000001234 или 123456789.000000.
    if (absoluteValue < 1e-4 || absoluteValue >= 1e8)
    {
        return QString::number(value, 'e', 5);
    }

    // В обычном случае сначала печатаем число с запасом знаков после запятой.
    // Затем уже на следующем шаге уберем все лишнее.
    QString text = QString::number(value, 'f', 6);
    // Затем убираем лишние завершающие нули и возможную финальную точку.
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.')))
    {
        // chop(1) удаляет один символ с конца строки.
        text.chop(1);
    }

    // Защищаемся от косметического случая "-0".
    if (text == "-0")
    {
        return "0";
    }

    return text;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ApplyInputPalette - ЕДИНАЯ ПАЛИТРА ДЛЯ ПОЛЕЙ ВВОДА
// ----------------------------------------------------------------------------
// Устанавливает согласованные цвета фона, текста, выделения и placeholder
// для всех интерактивных числовых полей и выпадающих списков.
//
// Принимает:
// - widget: указатель на виджет ввода, которому нужно задать палитру.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
template <typename TWidget>
void ApplyInputPalette(TWidget* widget)
{
    // Берем текущую палитру виджета как основу,
    // а не создаем палитру с нуля.
    QPalette palette = widget->palette();
    // Base - основной фон поля ввода.
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    // Text - обычный текст внутри поля.
    palette.setColor(QPalette::Text, QColor("#102033"));
    // WindowText - текстовые подписи, которые некоторые виджеты
    // могут использовать внутри себя.
    palette.setColor(QPalette::WindowText, QColor("#102033"));
    // ButtonText - цвет текста на встроенных кнопках, если они есть.
    palette.setColor(QPalette::ButtonText, QColor("#102033"));
    // Highlight - цвет выделения текста или активного выбора.
    palette.setColor(QPalette::Highlight, QColor("#2a6df4"));
    // HighlightedText - цвет текста на фоне выделения.
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    // PlaceholderText - цвет текста-подсказки в пустом поле.
    palette.setColor(QPalette::PlaceholderText, QColor("#7a8a9c"));
    // Применяем собранную палитру к виджету.
    widget->setPalette(palette);
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ CreateSpinField - СОЗДАНИЕ ЦЕЛОЧИСЛЕННОГО ПОЛЯ С КНОПКАМИ +/- 
// ----------------------------------------------------------------------------
// Возвращает готовый составной виджет:
// - слева сам spinbox;
// - справа вертикальная колонка кнопок "+" и "-".
// Такой подход дает более аккуратный внешний вид, чем стандартные кнопки Qt.
//
// Принимает:
// - spinBox: ссылку на указатель, в который будет записан созданный QSpinBox;
// - initialValue: начальное значение поля;
// - placeholder: текст-подсказку внутри поля;
// - parent: родительский виджет.
//
// Возвращает:
// - контейнер QWidget, внутри которого собраны поле и кнопки +/-.
// ----------------------------------------------------------------------------
QWidget* CreateSpinField(QSpinBox*& spinBox, int initialValue, const QString& placeholder, QWidget* parent)
{
    // Контейнер объединяет поле ввода и отдельную колонку кнопок.
    auto* container = new QWidget(parent);
    // Горизонтальная компоновка:
    // слева поле, справа кнопки.
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Создаем само целочисленное поле и заполняем его стартовым значением.
    spinBox = new IntegerSpinBox(container);
    spinBox->setValue(initialValue);
    // Устанавливаем текст-подсказку.
    static_cast<IntegerSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    // Отдельный мини-контейнер для вертикального блока кнопок.
    auto* buttonsHost = new QWidget(container);
    buttonsHost->setObjectName("spinButtonColumn");
    // Внутри него размещаем кнопки одна над другой.
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(4);

    // Создаем кнопки увеличения и уменьшения значения.
    auto* plusButton = new QPushButton("+", buttonsHost);
    auto* minusButton = new QPushButton("-", buttonsHost);
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");
    plusButton->setFixedSize(30, 18);
    minusButton->setFixedSize(30, 18);

    // Кнопки вызывают стандартный шаг вверх/вниз у spinbox.
    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepUp(); });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepDown(); });

    // Складываем кнопки в колонку.
    buttonsLayout->addWidget(plusButton);
    buttonsLayout->addWidget(minusButton);
    // addStretch прижимает кнопки к верху колонки.
    buttonsLayout->addStretch();

    // Добавляем в общую строку сначала поле, затем колонку кнопок.
    layout->addWidget(spinBox, 1);
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);
    // Возвращаем собранный составной виджет.
    return container;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ CreateSpinField - СОЗДАНИЕ ВЕЩЕСТВЕННОГО ПОЛЯ С КНОПКАМИ +/- 
// ----------------------------------------------------------------------------
// Это вещественный вариант предыдущей функции для параметров типа Δy и α.
//
// Принимает:
// - spinBox: ссылку на указатель, в который будет записан созданный QDoubleSpinBox;
// - initialValue: начальное значение поля;
// - placeholder: текст-подсказку внутри поля;
// - parent: родительский виджет.
//
// Возвращает:
// - контейнер QWidget, внутри которого собраны поле и кнопки +/-.
// ----------------------------------------------------------------------------
QWidget* CreateSpinField(QDoubleSpinBox*& spinBox, double initialValue, const QString& placeholder, QWidget* parent)
{
    // Контейнер объединяет поле вещественного ввода и колонку кнопок +/-.
    auto* container = new QWidget(parent);
    // Горизонтальная компоновка общей строки.
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Создаем само поле вещественного ввода.
    spinBox = new DecimalSpinBox(container);
    spinBox->setValue(initialValue);
    // Передаем placeholder в его внутренний lineEdit.
    static_cast<DecimalSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    // Контейнер под вертикальный стек кнопок.
    auto* buttonsHost = new QWidget(container);
    buttonsHost->setObjectName("spinButtonColumn");
    // Компоновка кнопок сверху вниз.
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(4);

    // Кнопки изменения значения вверх и вниз.
    auto* plusButton = new QPushButton("+", buttonsHost);
    auto* minusButton = new QPushButton("-", buttonsHost);
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");
    plusButton->setFixedSize(30, 18);
    minusButton->setFixedSize(30, 18);

    // Кнопки вызывают изменение значения стандартным шагом вверх/вниз.
    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepUp(); });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepDown(); });

    // Укладываем кнопки в колонку.
    buttonsLayout->addWidget(plusButton);
    buttonsLayout->addWidget(minusButton);
    // Пустое растяжение удерживает кнопки у верхнего края.
    buttonsLayout->addStretch();

    // Собираем строку "поле + кнопки".
    layout->addWidget(spinBox, 1);
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);
    // Возвращаем готовый составной виджет.
    return container;
}

// Возвращает человекочитаемое название метода моделирования.
//
// Принимает:
// - method: перечислимое значение метода моделирования.
//
// Возвращает:
// - строку с названием метода для интерфейса.
QString MethodTitle(SamplingMethod method)
{
    // В зависимости от значения enum возвращаем название метода,
    // которое затем используется в подписях графиков и интерфейса.
    return method == SamplingMethod::InverseFunction ? "Метод обратной функции"
                                                     : "Метод исключения";
}

// Включает/отключает либо само поле, либо его внешний контейнер.
// Это полезно для составных виджетов со встроенными кнопками.
//
// Принимает:
// - field: указатель на внутреннее поле или составной виджет;
// - enabled: нужно ли сделать поле доступным.
//
// Возвращает:
// - ничего.
void SetFieldContainerEnabled(QWidget* field, bool enabled)
{
    // Если поля нет, делать ничего не нужно.
    if (field == nullptr)
    {
        return;
    }

    // Если у поля есть внешний контейнер,
    // включаем/отключаем именно его целиком.
    // Это удобно для составных виджетов, где рядом с полем есть кнопки.
    if (auto* container = field->parentWidget())
    {
        container->setEnabled(enabled);
    }
    else
    {
        // Если контейнера нет, работаем напрямую с самим виджетом.
        field->setEnabled(enabled);
    }
}

// Строит ломаную линию, повторяющую контур гистограммы по готовым интервалам.
//
// Принимает:
// - histogram: массив интервалов гистограммы с оценками плотности.
//
// Возвращает:
// - набор точек для отрисовки контура гистограммы на графике.
QVector<QPointF> BuildHistogramOutline(const std::vector<HistogramBin>& histogram)
{
    // Контейнер для точек будущей ломаной.
    QVector<QPointF> points;
    if (histogram.empty())
    {
        return points;
    }

    // На каждый интервал нужно несколько точек:
    // подняться вверх, пройти по верхней границе и опуститься вниз.
    // Поэтому заранее резервируем память с запасом.
    points.reserve(static_cast<int>(histogram.size() * 4 + 2));
    // Начинаем с левого нижнего угла первой ступеньки.
    points.append(QPointF(histogram.front().left, 0.0));

    for (const HistogramBin& bin : histogram)
    {
        // Поднимаемся вертикально к высоте текущего интервала.
        points.append(QPointF(bin.left, bin.density));
        // Идем горизонтально до правой границы интервала.
        points.append(QPointF(bin.right, bin.density));
        // Опускаемся обратно к оси Ox.
        points.append(QPointF(bin.right, 0.0));
    }

    // Возвращаем набор точек, из которых графический виджет
    // построит ломаную линию контура гистограммы.
    return points;
}

// Строит набор точек для теоретической плотности f(x)=2x.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - набор точек теоретической плотности для графика.
QVector<QPointF> BuildDensityCurvePoints()
{
    // Контейнер для точек теоретической кривой плотности.
    QVector<QPointF> points;
    // Число шагов дискретизации на отрезке [0, 1].
    // Этого достаточно, чтобы линия выглядела гладкой.
    constexpr int kPointCount = 240;
    // Заранее резервируем память под все точки.
    points.reserve(kPointCount + 1);

    for (int index = 0; index <= kPointCount; ++index)
    {
        // Равномерно строим сетку точек на всём рабочем интервале.
        const double x = static_cast<double>(index) / static_cast<double>(kPointCount);
        // В каждой точке x вычисляем теоретическую плотность f(x)
        // и сохраняем пару координат для графика.
        points.append(QPointF(x, TVariantDistribution::Density(x)));
    }

    // Возвращаем набор точек, который графический виджет соединит линией.
    return points;
}

// Строит набор точек для теоретической функции распределения F(x).
//
// Принимает:
// - ничего.
//
// Возвращает:
// - набор точек теоретической функции распределения.
QVector<QPointF> BuildTheoreticalDistributionPoints()
{
    // Контейнер для точек теоретической функции распределения.
    QVector<QPointF> points;
    // Число шагов дискретизации по отрезку [0, 1].
    constexpr int kPointCount = 240;
    // Заранее резервируем память, чтобы избежать лишних перераспределений.
    points.reserve(kPointCount + 1);

    for (int index = 0; index <= kPointCount; ++index)
    {
        // Равномерно пробегаем весь рабочий отрезок.
        const double x = static_cast<double>(index) / static_cast<double>(kPointCount);
        // В каждой точке вычисляем теоретическое значение F(x).
        points.append(QPointF(x, TVariantDistribution::Distribution(x)));
    }

    // Возвращаем набор точек теоретической кривой распределения.
    return points;
}

// Строит ступенчатую эмпирическую функцию распределения по отсортированной выборке.
// Для очень больших выборок функция дополнительно прореживается,
// чтобы график оставался отзывчивым.
//
// Принимает:
// - sortedSamples: выборку, отсортированную по возрастанию.
//
// Возвращает:
// - набор точек ступенчатой эмпирической функции распределения.
QVector<QPointF> BuildEmpiricalDistributionPoints(const std::vector<double>& sortedSamples)
{
    // Контейнер для точек ступенчатой эмпирической функции распределения.
    QVector<QPointF> points;
    if (sortedSamples.empty())
    {
        return points;
    }

    // Для небольших выборок строим график по всем значениям.
    // Для больших выборок прореживаем точки, чтобы не перегружать отрисовку.
    const std::size_t rowStep =
        sortedSamples.size() <= kMaxEmpiricalPlotSteps
            ? 1
            : ((sortedSamples.size() - 1) / (kMaxEmpiricalPlotSteps - 1) + 1);
    // Оцениваем число шагов после прореживания.
    const std::size_t estimatedSteps = (sortedSamples.size() - 1) / rowStep + 1;

    // Для каждой ступеньки нужно как минимум две точки,
    // плюс стартовая и, возможно, финальная.
    points.reserve(static_cast<int>(estimatedSteps * 2 + 2));
    // До первой точки выборки эмпирическая функция равна нулю.
    points.append(QPointF(TVariantDistribution::SupportMin(), 0.0));

    // Размер выборки далее нужен для формул i / n.
    const double sampleSize = static_cast<double>(sortedSamples.size());
    for (std::size_t index = 0; index < sortedSamples.size(); index += rowStep)
    {
        // Текущее значение x_(i) из отсортированной выборки.
        const double x = sortedSamples[index];
        // Значение эмпирической функции непосредственно перед скачком.
        const double lowerValue = static_cast<double>(index) / sampleSize;
        // Значение эмпирической функции сразу после скачка.
        const double upperValue = static_cast<double>(index + 1) / sampleSize;

        // Добавляем вертикальный скачок в точке x.
        points.append(QPointF(x, lowerValue));
        points.append(QPointF(x, upperValue));
    }

    // Если последняя точка выборки не попала из-за прореживания,
    // вручную доводим график до конца.
    if (((sortedSamples.size() - 1) % rowStep) != 0)
    {
        const std::size_t lastIndex = sortedSamples.size() - 1;
        const double x = sortedSamples[lastIndex];
        const double lowerValue = static_cast<double>(lastIndex) / sampleSize;
        points.append(QPointF(x, lowerValue));
        points.append(QPointF(x, 1.0));
    }

    // После прохождения всей выборки функция должна закончиться в точке F = 1.
    points.append(QPointF(TVariantDistribution::SupportMax(), 1.0));
    return points;
}

// Строит последовательность значений выборки как график x_i по номеру элемента.
// Для больших выборок также выполняется прореживание числа точек.
//
// Принимает:
// - samples: исходную выборку в порядке генерации.
//
// Возвращает:
// - набор точек графика последовательности x_i.
QVector<QPointF> BuildSampleSequence(const std::vector<double>& samples)
{
    // Контейнер для точек графика последовательности x_i.
    QVector<QPointF> points;
    if (samples.empty())
    {
        return points;
    }

    // Верхняя граница числа точек на графике,
    // чтобы не перегрузить интерфейс на очень больших выборках.
    const std::size_t maxPoints = 5000;
    // Если выборка небольшая, используем все точки.
    // Иначе строим график с шагом.
    const std::size_t step =
        samples.size() <= maxPoints ? 1 : ((samples.size() - 1) / (maxPoints - 1) + 1);

    // Оцениваем размер массива точек после прореживания.
    const std::size_t estimatedSize = (samples.size() - 1) / step + 1;
    // Заранее резервируем память.
    points.reserve(static_cast<int>(estimatedSize + 1));

    for (std::size_t index = 0; index < samples.size(); index += step)
    {
        // По оси X откладываем номер элемента,
        // по оси Y - его значение в выборке.
        points.append(QPointF(static_cast<double>(index + 1), samples[index]));
    }

    // Если последняя точка не попала в прореженный проход,
    // добавляем ее явно, чтобы график доходил до конца выборки.
    if (points.isEmpty() || points.back().x() != static_cast<double>(samples.size()))
    {
        points.append(QPointF(static_cast<double>(samples.size()), samples.back()));
    }

    // Возвращаем набор точек последовательности x_i.
    return points;
}

// ----------------------------------------------------------------------------
// КЛАСС SampleTableModel - МОДЕЛЬ ТАБЛИЦЫ ДЛЯ ОТОБРАЖЕНИЯ ВЫБОРКИ
// ----------------------------------------------------------------------------
// Таблица показывает:
// - номер элемента;
// - исходное значение x_i;
// - соответствующий элемент отсортированной выборки x_(i);
// - значение эмпирической функции распределения.
//
// Чтобы таблица оставалась отзывчивой на больших выборках,
// строки не "выкладываются" все сразу.
// Модель отдает их частями через canFetchMore/fetchMore,
// но при этом логически доступна вся выборка целиком.
// ----------------------------------------------------------------------------
class SampleTableModel final : public QAbstractTableModel
{
public:
    // Базовый конструктор модели.
    //
    // Принимает:
    // - parent: родительский QObject.
    //
    // Возвращает:
    // - созданный объект SampleTableModel.
    explicit SampleTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent)
    {
    }

    // Подключает к модели новый результат моделирования.
    //
    // Принимает:
    // - result: указатель на рассчитанный результат моделирования.
    //
    // Возвращает:
    // - ничего.
    void SetResult(const SamplingResult* result)
    {
        // beginResetModel/endResetModel сообщают представлению,
        // что структура данных модели сейчас будет полностью перестроена.
        beginResetModel();
        // Сохраняем указатель на новый результат.
        result_ = result;
        // При смене результата начинаем загрузку строк заново.
        loadedRowCount_ = 0;

        if (result_ != nullptr)
        {
            // Сразу открываем первую порцию строк,
            // чтобы таблица не была пустой после расчета.
            loadedRowCount_ = std::min<std::size_t>(result_->samples.size(),
                                                    static_cast<std::size_t>(kTableFetchChunkSize));
        }

        // Завершаем полную перезагрузку модели.
        endResetModel();
    }

    // Число строк таблицы.
    //
    // Принимает:
    // - parent: родительский индекс Qt Model/View.
    //
    // Возвращает:
    // - число строк, уже открытых таблице в текущий момент.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        // Для дочерних элементов и при отсутствии результата строк нет.
        if (parent.isValid() || result_ == nullptr)
        {
            return 0;
        }

        // Возвращаем именно число уже загруженных строк,
        // а не полный размер выборки.
        return static_cast<int>(loadedRowCount_);
    }

    // Число столбцов таблицы.
    //
    // Принимает:
    // - parent: родительский индекс Qt Model/View.
    //
    // Возвращает:
    // - количество столбцов таблицы.
    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        // Таблица плоская, без дочерних элементов.
        return parent.isValid() ? 0 : 4;
    }

    // Возвращает данные ячейки или ее вспомогательные свойства.
    //
    // Принимает:
    // - index: индекс ячейки;
    // - role: тип запрашиваемых данных.
    //
    // Возвращает:
    // - содержимое ячейки или служебные данные для Qt Model/View.
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        // Если индекс некорректен или результат не подключен,
        // возвращаем пустое значение.
        if (!index.isValid() || result_ == nullptr)
        {
            return {};
        }

        // Номер строки напрямую соответствует индексу элемента выборки,
        // потому что таблица теперь показывает полные данные без прореживания.
        const std::size_t sampleIndex = static_cast<std::size_t>(index.row());

        // Все значения в таблице центрируем для аккуратного вида.
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }

        // Все остальные роли, кроме отображения текста, здесь не обрабатываем.
        if (role != Qt::DisplayRole)
        {
            return {};
        }

        // В зависимости от номера столбца возвращаем нужное представление данных.
        switch (index.column())
        {
        // Номер строки в исходной выборке.
        case 0:
            return static_cast<qulonglong>(sampleIndex + 1);
        // Исходное значение x_i.
        case 1:
            return FormatNumericValue(result_->samples[sampleIndex]);
        // Соответствующее отсортированное значение x_(i).
        case 2:
            return FormatNumericValue(result_->sortedSamples[sampleIndex]);
        // Эмпирическая функция распределения в табличной форме.
        case 3:
            return FormatNumericValue(static_cast<double>(sampleIndex) /
                                      static_cast<double>(result_->sortedSamples.size()));
        default:
            return {};
        }
    }

    // Возвращает подписи заголовков таблицы.
    //
    // Принимает:
    // - section: номер столбца или строки;
    // - orientation: ориентацию заголовка;
    // - role: роль запрашиваемых данных.
    //
    // Возвращает:
    // - подпись заголовка или служебные данные выравнивания.
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        // Центрируем подписи заголовков.
        if (role == Qt::TextAlignmentRole)
        {
            return Qt::AlignCenter;
        }

        // Пользовательские подписи задаем только для горизонтального заголовка.
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        {
            return QAbstractTableModel::headerData(section, orientation, role);
        }

        // Тексты заголовков столбцов.
        static const QStringList headers = {
            "i",
            "xᵢ",
            "x₍ᵢ₎",
            "Fэмп(x₍ᵢ₎)"};

        // Защищаемся от выхода за границы массива заголовков.
        if (section < 0 || section >= headers.size())
        {
            return {};
        }

        // Возвращаем подпись нужного столбца.
        return headers[section];
    }

    // Сообщает Qt, есть ли еще строки, которые пока не загружены в таблицу.
    //
    // Принимает:
    // - parent: родительский индекс Qt Model/View.
    //
    // Возвращает:
    // - true, если можно догрузить еще строки;
    // - false, если все строки уже открыты.
    bool canFetchMore(const QModelIndex& parent) const override
    {
        // Для дочерних элементов и при отсутствии результата догружать нечего.
        if (parent.isValid() || result_ == nullptr)
        {
            return false;
        }

        // Пока число загруженных строк меньше полного размера выборки,
        // таблица может запросить следующую порцию.
        return loadedRowCount_ < result_->samples.size();
    }

    // Догружает следующую порцию строк по мере прокрутки таблицы.
    //
    // Принимает:
    // - parent: родительский индекс Qt Model/View.
    //
    // Возвращает:
    // - ничего.
    void fetchMore(const QModelIndex& parent) override
    {
        // Дочерние элементы не поддерживаются, а без результата загружать нечего.
        if (parent.isValid() || result_ == nullptr)
        {
            return;
        }

        // Полный размер выборки, доступный модели.
        const std::size_t totalRowCount = result_->samples.size();
        // Если уже всё загружено, выходим.
        if (loadedRowCount_ >= totalRowCount)
        {
            return;
        }

        // Считаем, сколько строк можно открыть в этой порции.
        const std::size_t fetchCount =
            std::min<std::size_t>(static_cast<std::size_t>(kTableFetchChunkSize),
                                  totalRowCount - loadedRowCount_);
        // Первая и последняя строки новой вставки.
        const int firstRow = static_cast<int>(loadedRowCount_);
        const int lastRow = static_cast<int>(loadedRowCount_ + fetchCount - 1);

        // beginInsertRows/endInsertRows корректно уведомляют представление,
        // что в модель добавляется новая группа строк.
        beginInsertRows(QModelIndex(), firstRow, lastRow);
        // После уведомления увеличиваем число уже открытых строк.
        loadedRowCount_ += fetchCount;
        endInsertRows();
    }

private:
    // Указатель на текущий результат моделирования.
    const SamplingResult* result_ = nullptr;
    // Сколько строк уже открыто таблице в данный момент.
    std::size_t loadedRowCount_ = 0;
};
} // namespace

// ----------------------------------------------------------------------------
// КОНСТРУКТОР MainWindow - СОЗДАНИЕ И ИНИЦИАЛИЗАЦИЯ ГЛАВНОГО ОКНА
// ----------------------------------------------------------------------------
// Принимает:
// - parent: родительский виджет Qt, обычно nullptr для главного окна.
//
// Возвращает:
// - полностью инициализированный объект MainWindow.
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    // Сначала физически создаем все виджеты и компоновки окна.
    BuildUi();
    // Затем применяем к уже созданным элементам общую визуальную тему.
    ApplyTheme();
    // Создаем наблюдатель фонового вычисления.
    // Он сообщит окну, когда моделирование завершится.
    simulationWatcher_ = new QFutureWatcher<SamplingResult>(this);
    // После создания всех объектов связываем сигналы и слоты.
    SetupSignals();
    // На старте окно не должно показывать старые/случайные результаты,
    // поэтому сразу очищаем сводку, графики и таблицу.
    ResetOutputs();
    // Показываем стартовый статус готовности к работе.
    ShowStatus("Готово к моделированию", true);
}

// ----------------------------------------------------------------------------
// МЕТОД BuildUi - ПОСТРОЕНИЕ ВСЕГО ИНТЕРФЕЙСА ГЛАВНОГО ОКНА
// ----------------------------------------------------------------------------
// Здесь последовательно создаются:
// - левая панель с параметрами и теорией;
// - правая панель с итогами, графиками и таблицей;
// - splitter между панелями;
// - все вложенные layout и виджеты.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::BuildUi()
{
    // Задаем базовые свойства окна.
    // Это заголовок окна и его стартовые размеры на экране.
    setWindowTitle("Лабораторная работа 1 — моделирование случайной величины");
    resize(1560, 940);
    setMinimumSize(1320, 820);

    // Центральный контейнер Qt, внутри которого живет все содержимое окна.
    // Именно он становится setCentralWidget для QMainWindow.
    auto* central = new QWidget(this);
    // Корневая вертикальная компоновка.
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(0);

    // Splitter делит окно на левую и правую панели,
    // позволяя пользователю менять их ширину.
    // Это удобно: параметры слева и результаты справа можно
    // подстроить под себя без изменения структуры окна.
    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(4);

    // Левая карточка с настройками моделирования.
    // Здесь находятся все входные параметры и краткая теория варианта.
    auto* controlCard = new QWidget(splitter);
    controlCard->setObjectName("controlCard");
    controlCard->setMinimumWidth(460);
    controlCard->setMaximumWidth(560);

    // Компоновка левой панели.
    // Элементы будут идти сверху вниз:
    // заголовок, параметры, теория, кнопка, статус.
    auto* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(18, 18, 18, 18);
    controlLayout->setSpacing(12);

    // Основной заголовок окна на панели управления.
    auto* titleLabel = new QLabel("Моделирование случайной величины", controlCard);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setWordWrap(true);

    // Группа параметров моделирования.
    // Это главный блок пользовательского ввода перед запуском расчета.
    auto* parametersGroup = new QGroupBox("Параметры моделирования", controlCard);
    parametersGroup->setObjectName("parametersGroup");
    // Form layout удобен для строк вида "подпись - поле".
    auto* parametersForm = new QFormLayout(parametersGroup);
    parametersForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    parametersForm->setFormAlignment(Qt::AlignTop);
    parametersForm->setContentsMargins(14, 18, 14, 14);
    parametersForm->setSpacing(10);
    parametersForm->setHorizontalSpacing(16);

    // Выпадающий список выбора метода моделирования.
    // От него зависит, какие поля должны быть видимыми,
    // и какая логика расчета будет запущена.
    methodComboBox_ = new MethodComboBox(parametersGroup);
    methodComboBox_->setObjectName("methodComboBox");
    methodComboBox_->setMinimumHeight(42);
    methodComboBox_->addItem("Метод обратной функции");
    methodComboBox_->addItem("Метод исключения");

    // Поле размера выборки n.
    // По этому числу вычислительное ядро поймет,
    // сколько значений случайной величины нужно сгенерировать.
    auto* sampleSizeField = CreateSpinField(sampleSizeEdit_, 10000, "10000", parametersGroup);
    sampleSizeEdit_->setRange(100, kMaxRecommendedSampleSize);
    sampleSizeEdit_->setSingleStep(5000);
    sampleSizeEdit_->setValue(10000);

    // Поле числа интервалов гистограммы.
    // Этот параметр нужен уже не для генерации выборки,
    // а для последующего представления результата на графике.
    auto* binsField = CreateSpinField(binsEdit_, 25, "25", parametersGroup);
    binsEdit_->setRange(5, kMaxRecommendedHistogramBins);
    binsEdit_->setSingleStep(10);
    binsEdit_->setValue(25);

    // Поле шага интегрирования Δy для метода обратной функции.
    // Оно влияет на точность численного накопления вероятности.
    auto* stepField = CreateSpinField(stepEdit_, 0.001, "0.001", parametersGroup);
    stepFieldContainer_ = stepField;
    stepEdit_->setDecimals(6);
    stepEdit_->setRange(0.000001, 0.1);
    stepEdit_->setSingleStep(0.0005);
    stepEdit_->setValue(0.001);

    // Поле уровня значимости α для критерия Колмогорова.
    // Этот параметр используется уже на этапе проверки гипотезы,
    // а не на этапе генерации выборки.
    auto* alphaField = CreateSpinField(alphaEdit_, 0.050, "0.050", parametersGroup);
    alphaEdit_->setDecimals(3);
    alphaEdit_->setRange(0.001, 0.250);
    alphaEdit_->setSingleStep(0.01);
    alphaEdit_->setValue(0.050);

    // Применяем одинаковую цветовую палитру ко всем полям ввода.
    ApplyInputPalette(methodComboBox_);
    ApplyInputPalette(sampleSizeEdit_);
    ApplyInputPalette(binsEdit_);
    ApplyInputPalette(stepEdit_);
    ApplyInputPalette(alphaEdit_);

    // Добавляем строки параметров в форму.
    // Здесь задается фактический порядок отображения полей в интерфейсе.
    parametersForm->addRow("Метод", methodComboBox_);
    parametersForm->addRow("Размер выборки n", sampleSizeField);
    parametersForm->addRow("Интервалы гистограммы", binsField);
    // Подпись шага Δy сохраняем отдельно,
    // потому что ее потом нужно скрывать/показывать вместе с полем.
    stepLabel_ = new QLabel("Шаг Δy", parametersGroup);
    parametersForm->addRow(stepLabel_, stepField);
    parametersForm->addRow("Уровень значимости α", alphaField);

    // Блок с краткой теорией по варианту.
    // Он нужен как визуальная шпаргалка:
    // пользователь сразу видит плотность, F(x) и основные характеристики.
    auto* theoryGroup = new QGroupBox("Теория варианта", controlCard);
    theoryGroup->setObjectName("theoryGroup");
    // Сетка удобна для пар "название характеристики - значение".
    auto* theoryLayout = new QGridLayout(theoryGroup);
    theoryLayout->setContentsMargins(18, 20, 18, 18);
    theoryLayout->setHorizontalSpacing(12);
    theoryLayout->setVerticalSpacing(10);
    theoryLayout->setColumnStretch(1, 1);

    // Локальный помощник для добавления одной строки в блок теории.
    // Он уменьшает дублирование кода:
    // каждая строка теории создается по одному и тому же шаблону.
    auto addTheoryRow = [theoryLayout, theoryGroup](int row,
                                                    const QString& title,
                                                    const QString& value,
                                                    bool richText = false,
                                                    QLabel** titleOut = nullptr,
                                                    QLabel** valueOut = nullptr) {
        // Левая подпись строки: название характеристики.
        auto* titleLabel = new QLabel(title, theoryGroup);
        titleLabel->setObjectName("theoryKey");
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        // Правая часть строки: значение характеристики или формула.
        auto* valueLabel = new QLabel(value, theoryGroup);
        valueLabel->setObjectName("theoryValue");
        valueLabel->setWordWrap(true);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        // Для строк с HTML-форматированием включаем rich text,
        // чтобы корректно отображались индексы, степени и спецсимволы.
        if (richText)
        {
            valueLabel->setTextFormat(Qt::RichText);
        }

        // Размещаем подпись и значение в одной строке сетки.
        theoryLayout->addWidget(titleLabel, row, 0, Qt::AlignLeft | Qt::AlignTop);
        theoryLayout->addWidget(valueLabel, row, 1);

        // При необходимости наружу можно вернуть указатель на левую подпись.
        if (titleOut != nullptr)
        {
            *titleOut = titleLabel;
        }

        // При необходимости наружу можно вернуть указатель на правое значение.
        // Это используется для строки, которая должна скрываться/показываться
        // в зависимости от выбранного метода.
        if (valueOut != nullptr)
        {
            *valueOut = valueLabel;
        }
    };

    // Заполняем блок теории формулами именно для текущего варианта.
    // Здесь не происходит вычислений:
    // это заранее заготовленные теоретические сведения по варианту,
    // которые пользователь видит как справочную часть интерфейса.
    addTheoryRow(0, "Плотность", "f(x)=2x, 0≤x≤1");
    addTheoryRow(1, "Функция распределения", "F(x)=x<sup>2</sup>, 0≤x≤1", true);
    addTheoryRow(2, "Математическое ожидание", "2/3 ≈ 0.6667");
    addTheoryRow(3, "Дисперсия", "1/18 ≈ 0.0556");
    addTheoryRow(4, "Медиана", "1/√2 ≈ 0.7071");
    addTheoryRow(5, "Мода", "1.0000");
    // Последняя строка относится только к методу исключения,
    // поэтому ее подписи сохраняются отдельно,
    // чтобы затем можно было скрывать/показывать их по выбранному методу.
    addTheoryRow(6,
                 "Для метода исключения",
                 "a=0, b=1, M=2, критерий принятия z&lt;y",
                 true,
                 &rejectionTheoryLabel_,
                 &rejectionTheoryValueLabel_);

    // Главная кнопка запуска расчета.
    // Именно с нее пользователь запускает моделирование по текущим параметрам.
    runButton_ = new QPushButton("Выполнить моделирование", controlCard);
    runButton_->setObjectName("primaryButton");
    runButton_->setMinimumHeight(46);

    // Статусная плашка под кнопкой запуска.
    // В ней будут появляться:
    // - сообщения об ошибках ввода;
    // - статус "идет моделирование";
    // - сообщение об успешном завершении расчета.
    statusBadge_ = new QLabel(controlCard);
    statusBadge_->setWordWrap(true);

    // Собираем левую панель сверху вниз.
    // Порядок важен, потому что именно так пользователь видит интерфейс:
    // сначала заголовок, потом ввод, затем теория и управляющие элементы.
    controlLayout->addWidget(titleLabel);
    controlLayout->addWidget(parametersGroup);
    controlLayout->addWidget(theoryGroup);
    controlLayout->addWidget(runButton_);
    controlLayout->addWidget(statusBadge_);
    controlLayout->addStretch();

    // Сразу на старте приводим интерфейс к виду,
    // соответствующему выбранному по умолчанию методу.
    UpdateMethodSpecificInputsVisibility();

    // Правая карточка с результатами.
    // Здесь будут отображаться все результаты после завершения расчета.
    auto* resultsCard = new QWidget(splitter);
    resultsCard->setObjectName("resultsCard");

    // Компоновка правой панели.
    // Сверху будет заголовок и сводка,
    // ниже - вкладки с детальными представлениями результата.
    auto* resultsLayout = new QVBoxLayout(resultsCard);
    resultsLayout->setContentsMargins(18, 18, 18, 18);
    resultsLayout->setSpacing(12);

    // Заголовок правой панели.
    auto* resultsTitle = new QLabel("Результаты моделирования", resultsCard);
    resultsTitle->setObjectName("resultsTitle");

    // Сводный блок итогов расчета.
    auto* summaryGroup = new QGroupBox("Итоги расчёта", resultsCard);
    summaryGroup->setObjectName("summaryGroup");
    auto* summaryLayout = new QGridLayout(summaryGroup);
    summaryLayout->setContentsMargins(14, 18, 14, 14);
    summaryLayout->setHorizontalSpacing(18);
    summaryLayout->setVerticalSpacing(10);

    // Локальный помощник для одной строки сводки.
    // Каждая строка состоит из:
    // - текстовой подписи;
    // - QLabel со значением, которое позже заполнится результатом расчета.
    auto addSummaryRow = [summaryLayout, resultsCard](int row,
                                                      int column,
                                                      const QString& title,
                                                      QLabel*& valueLabel)
    {
        // Подпись характеристики.
        auto* caption = new QLabel(title, resultsCard);
        caption->setObjectName("summaryCaption");
        // Поле, в которое потом будет подставлено вычисленное значение.
        // На старте там стоит тире как признак отсутствия результата.
        valueLabel = new QLabel("—", resultsCard);
        valueLabel->setObjectName("summaryValue");

        // Размещаем подпись и значение в сетке.
        summaryLayout->addWidget(caption, row, column * 2);
        summaryLayout->addWidget(valueLabel, row, column * 2 + 1);
    };

    // Заполняем сводный блок подписями выходных характеристик.
    // Это именно каркас сводки.
    // Числа появятся позже в UpdateSummary(...), после завершения моделирования.
    addSummaryRow(0, 0, "Размер выборки", sampleCountValue_);
    addSummaryRow(0, 1, "Среднее", meanValue_);
    addSummaryRow(1, 0, "Дисперсия", varianceValue_);
    addSummaryRow(1, 1, "Медиана", medianValue_);
    addSummaryRow(2, 0, "D<sub>n</sub>", dnValue_);
    addSummaryRow(2, 1, "K<sub>n</sub>", knValue_);
    addSummaryRow(3, 0, "K<sub>1-α</sub>", criticalValue_);
    addSummaryRow(3, 1, "Гипотеза H<sub>0</sub>", hypothesisValue_);
    addSummaryRow(4, 0, "Доля принятия", acceptanceValue_);

    // Контейнер вкладок с различными представлениями результата.
    // Каждая вкладка отвечает за свой способ просмотра результата:
    // графики, последовательность или таблица.
    resultsTabs_ = new QTabWidget(resultsCard);
    resultsTabs_->setObjectName("resultsTabs");
    resultsTabs_->setMovable(true);

    // Вкладка гистограммы и теоретической плотности.
    // Здесь сравнивается эмпирическая оценка плотности по выборке
    // с теоретической плотностью варианта.
    auto* histogramTab = new QWidget(resultsTabs_);
    auto* histogramLayout = new QVBoxLayout(histogramTab);
    histogramLayout->setContentsMargins(0, 0, 0, 0);
    histogramPlot_ = new PlotChartWidget("Гистограмма и теоретическая плотность", "x", "плотность", histogramTab);
    histogramLayout->addWidget(histogramPlot_);

    // Вкладка эмпирической и теоретической функций распределения.
    // Она позволяет визуально сравнить Fэмп(x) и F(x),
    // что напрямую связано с критерием Колмогорова.
    auto* distributionTab = new QWidget(resultsTabs_);
    auto* distributionLayout = new QVBoxLayout(distributionTab);
    distributionLayout->setContentsMargins(0, 0, 0, 0);
    cdfPlot_ =
        new PlotChartWidget("Выборочная и теоретическая функции распределения", "x", "F(x)", distributionTab);
    distributionLayout->addWidget(cdfPlot_);

    // Вкладка последовательности элементов выборки.
    // На ней видно, как выглядят сами сгенерированные значения в порядке появления.
    auto* sampleTab = new QWidget(resultsTabs_);
    auto* sampleLayout = new QVBoxLayout(sampleTab);
    sampleLayout->setContentsMargins(0, 0, 0, 0);
    samplePlot_ = new PlotChartWidget("Последовательность сгенерированных значений", "номер элемента", "x", sampleTab);
    sampleLayout->addWidget(samplePlot_);

    // Вкладка табличного представления выборки.
    // Это более "точный" способ просмотра тех же данных,
    // которые на графиках уже показаны в агрегированном виде.
    auto* tableTab = new QWidget(resultsTabs_);
    auto* tableLayout = new QVBoxLayout(tableTab);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    // Внутренняя карточка таблицы оформляется так же, как карточки графиков,
    // чтобы все вкладки выглядели единообразно.
    auto* tableCard = new QFrame(tableTab);
    tableCard->setObjectName("plotCard");
    auto* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(12, 12, 12, 12);
    tableCardLayout->setSpacing(10);

    auto* tableTitle = new QLabel("Таблица выборки", tableCard);
    tableTitle->setObjectName("tableTitle");
    // Создаем саму таблицу и подключаем к ней нашу модель данных.
    samplesTable_ = new QTableView(tableCard);
    samplesModel_ = new SampleTableModel(samplesTable_);
    samplesTable_->setModel(samplesModel_);
    // Чередование цветов строк улучшает читаемость длинной таблицы.
    samplesTable_->setAlternatingRowColors(true);
    // Выделяем сразу целую строку, а не отдельную ячейку.
    samplesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    samplesTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    // Редактирование запрещено: таблица показывает результат, а не принимает ввод.
    samplesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Не переносим текст по строкам, чтобы таблица оставалась ровной.
    samplesTable_->setWordWrap(false);
    // Вертикальный заголовок скрываем, потому что номер строки
    // у нас уже есть в первом столбце.
    samplesTable_->verticalHeader()->setVisible(false);
    // Все столбцы растягиваются по доступной ширине.
    samplesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    samplesTable_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Собираем карточку таблицы.
    tableCardLayout->addWidget(tableTitle);
    tableCardLayout->addWidget(samplesTable_);
    tableLayout->addWidget(tableCard);

    // Добавляем вкладки в контейнер результатов.
    // Порядок вкладок определяет и порядок, в котором пользователь их видит.
    resultsTabs_->addTab(histogramTab, "Гистограмма");
    resultsTabs_->addTab(distributionTab, "Функция F(x)");
    resultsTabs_->addTab(sampleTab, "Выборка");
    resultsTabs_->addTab(tableTab, "Таблица");

    // Собираем правую панель.
    // Сначала заголовок, потом краткая сводка, затем детальные вкладки.
    resultsLayout->addWidget(resultsTitle);
    resultsLayout->addWidget(summaryGroup);
    resultsLayout->addWidget(resultsTabs_, 1);

    // Подключаем обе панели к splitter.
    // Левая панель отвечает за ввод, правая - за отображение результата.
    splitter->addWidget(controlCard);
    splitter->addWidget(resultsCard);
    // Правая панель получает растягивание,
    // потому что графикам и таблице обычно нужно больше места.
    splitter->setStretchFactor(1, 1);
    // Стартовое соотношение ширин левой и правой части.
    splitter->setSizes({520, 980});

    // Завершаем сборку центрального содержимого окна.
    // Добавляем splitter в корневую компоновку
    // и назначаем центральный виджет главному окну.
    rootLayout->addWidget(splitter);
    setCentralWidget(central);
}

// ----------------------------------------------------------------------------
// МЕТОД ApplyTheme - ПРИМЕНЕНИЕ ВИЗУАЛЬНОЙ ТЕМЫ ОКНА
// ----------------------------------------------------------------------------
// Весь внешний вид окна задается единой QSS-строкой:
// цвета, отступы, шрифты, кнопки, вкладки, таблица и scroll bar.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ApplyTheme()
{
    // Весь внешний вид окна задаем одной QSS-строкой.
    // Это позволяет централизованно управлять:
    // - цветами;
    // - размерами шрифтов;
    // - радиусами скругления;
    // - оформлением полей, вкладок, таблиц и скроллбаров.
    setStyleSheet(R"(
        /* -----------------------------------------------------------------
           Базовый фон главного окна и карточек двух панелей
           ----------------------------------------------------------------- */
        /* QMainWindow - общий фон всего приложения за пределами карточек. */
        QMainWindow {
            background: #eef3f8;
        }

        /* Селектор QWidget#... применяется только к виджетам
           с соответствующим objectName.
           Здесь так оформляются две основные карточки интерфейса:
           левая панель управления и правая панель результатов. */
        QWidget#controlCard, QWidget#resultsCard {
            background: #fbfdff;
            border: 1px solid #d8e2ed;
            border-radius: 18px;
        }

        /* -----------------------------------------------------------------
           Разделитель между левой и правой панелями
           ----------------------------------------------------------------- */
        /* handle - это перетаскиваемая полоса splitter.
           Ей задаем мягкий цвет и небольшое скругление. */
        QSplitter::handle {
            background: #dbe6f2;
            margin: 20px 0;
            border-radius: 2px;
        }

        /* При наведении делаем handle немного контрастнее,
           чтобы пользователь видел, что его можно тянуть. */
        QSplitter::handle:hover {
            background: #c5d5e6;
        }

        /* -----------------------------------------------------------------
           Общие правила для текстовых подписей и заголовков
           ----------------------------------------------------------------- */
        /* Базовый стиль для большинства QLabel в окне. */
        QLabel {
            color: #102033;
            font-size: 15px;
        }

        /* titleLabel и resultsTitle - это два главных заголовка
           над левой и правой панелью соответственно. */
        QLabel#titleLabel, QLabel#resultsTitle {
            color: #102033;
            font-size: 24px;
            font-weight: 700;
        }

        /* tableTitle - заголовок карточки таблицы на одноименной вкладке. */
        QLabel#tableTitle {
            color: #102033;
            font-size: 18px;
            font-weight: 700;
        }

        /* summaryCaption - левая подпись в сводке:
           название статистики или характеристики. */
        QLabel#summaryCaption {
            color: #4d647a;
            font-size: 15px;
            font-weight: 600;
        }

        /* summaryValue - правое значение в сводке:
           число или текст решения по гипотезе. */
        QLabel#summaryValue {
            color: #102033;
            font-size: 15px;
            font-weight: 700;
        }

        /* -----------------------------------------------------------------
           Карточки графиков и заголовки внутри них
           ----------------------------------------------------------------- */
        /* plotCard - белая внутренняя карточка, в которой живут график
           или таблица на соответствующей вкладке. */
        QFrame#plotCard {
            border: 1px solid #d9e2ec;
            border-radius: 16px;
            background: #ffffff;
        }

        /* plotTitle - заголовок внутри карточки графика. */
        QLabel#plotTitle {
            color: #102033;
            font-size: 16px;
            font-weight: 700;
        }

        /* У самого QChartView убираем фон и рамку,
           чтобы видимой оставалась именно карточка-контейнер. */
        QChartView#plotChartView {
            background: transparent;
            border: none;
        }

        /* -----------------------------------------------------------------
           Общий стиль групповых блоков: параметры, теория, итоги
           ----------------------------------------------------------------- */
        /* Базовый стиль любого QGroupBox:
           рамка, скругление, белый фон и общий шрифт заголовка. */
        QGroupBox {
            color: #102033;
            font-size: 14px;
            font-weight: 700;
            border: 1px solid #d9e2ec;
            border-radius: 14px;
            margin-top: 12px;
            background: #ffffff;
        }

        /* QGroupBox::title - это специальный subcontrol заголовка группы.
           subcontrol-origin: margin означает, что позиционирование идет
           относительно внешнего поля группы.
           left и padding задают смещение и внутренние отступы заголовка. */
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }

        /* Делаем заголовки основных групп крупнее и выразительнее. */
        /* У этих трех блоков увеличиваем верхний отступ,
           чтобы крупный заголовок не прилипал к рамке. */
        QGroupBox#parametersGroup,
        QGroupBox#summaryGroup,
        QGroupBox#theoryGroup {
            margin-top: 18px;
        }

        /* Специальный стиль заголовков параметров и итогов:
           крупный размер, насыщенный вес и более глубокий цвет. */
        QGroupBox#parametersGroup::title,
        QGroupBox#summaryGroup::title {
            color: #17324d;
            font-size: 26px;
            font-weight: 800;
            left: 16px;
            padding: 0 10px 2px 10px;
        }

        /* Для блока теории используем такой же визуальный акцент,
           но отдельным правилом, чтобы можно было управлять им независимо. */
        QGroupBox#theoryGroup::title {
            color: #17324d;
            font-size: 26px;
            font-weight: 800;
            left: 16px;
            padding: 0 10px 2px 10px;
        }

        /* Отдельная типографика для содержимого блока теории. */
        /* Подписи внутри блока параметров делаем чуть темнее и плотнее,
           чтобы они уверенно читались рядом с полями ввода. */
        QGroupBox#parametersGroup QLabel {
            font-size: 15px;
            font-weight: 600;
            color: #20384f;
        }

        /* theoryKey - левая колонка блока теории:
           названия характеристик вроде "Плотность" или "Дисперсия". */
        QLabel#theoryKey {
            color: #294560;
            font-size: 15px;
            font-weight: 700;
        }

        /* theoryValue - правая колонка блока теории:
           формулы и численные значения. */
        QLabel#theoryValue {
            color: #102033;
            font-size: 15px;
            font-weight: 500;
        }

        /* -----------------------------------------------------------------
           Общий стиль полей ввода
           ----------------------------------------------------------------- */
        /* Это базовое правило сразу для четырех типов полей:
           обычного line edit, combobox и двух видов spinbox.
           Здесь задаются:
           - общий размер шрифта;
           - высота поля;
           - рамка и скругление;
           - внутренние отступы;
           - цвета текста, фона и выделения. */
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

        /* :focus срабатывает, когда поле активно и имеет клавиатурный фокус.
           Здесь мы просто подсвечиваем рамку синим,
           чтобы пользователь видел активный элемент ввода. */
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #2a6df4;
        }

        /* -----------------------------------------------------------------
           Внешние кнопки +/- у числовых полей
           ----------------------------------------------------------------- */
        /* spinAdjustButton - это наши отдельные кнопки шага,
           размещенные рядом с полем ввода.
           Они стилизуются как компактные светлые управляющие кнопки. */
        QPushButton#spinAdjustButton {
            background: #f7fbff;
            color: #1e3655;
            border: 1px solid #d7e2ee;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 700;
            padding: 0;
        }

        /* :hover - состояние наведения курсора. */
        QPushButton#spinAdjustButton:hover {
            background: #edf5ff;
            border-color: #bdd0e6;
        }

        /* :pressed - состояние нажатия кнопки. */
        QPushButton#spinAdjustButton:pressed {
            background: #dfeaf8;
        }

        /* -----------------------------------------------------------------
           Общая зона выпадающего элемента у combobox
           ----------------------------------------------------------------- */
        /* QComboBox::drop-down - специальный subcontrol правой части combobox,
           где находится стрелка раскрытия.
           subcontrol-origin: padding означает, что позиционирование ведется
           относительно внутренней области с учетом padding.
           subcontrol-position: top right прижимает drop-down к правому краю. */
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #d7e2ee;
            background: #f7fbff;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }

        /* -----------------------------------------------------------------
           Специальное оформление combobox выбора метода
           ----------------------------------------------------------------- */
        /* Отдельное правило для methodComboBox позволяет:
           - отличить его от остальных полей;
           - точнее настроить внутренние отступы и стрелку;
           - управлять popup через пользовательское свойство popupOpen. */
        QComboBox#methodComboBox {
            /* combobox-popup: 0 оставляет popup в обычном режиме Qt,
               без "слипания" с виджетом как у некоторых платформенных тем. */
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

        /* У drop-down внутри methodComboBox делаем отдельную ширину,
           чтобы стрелка смотрелась аккуратнее. */
        QComboBox#methodComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: 1px solid #d7e2ee;
            background: #f7fbff;
            border-top-right-radius: 10px;
            border-bottom-right-radius: 10px;
        }

        /* Стандартное состояние стрелки: список закрыт, стрелка смотрит вниз. */
        QComboBox#methodComboBox::down-arrow {
            image: url(:/icons/arrow-down.svg);
            width: 12px;
            height: 12px;
        }

        /* Если пользовательское свойство popupOpen=true,
           меняем иконку на стрелку вверх. */
        QComboBox#methodComboBox[popupOpen="true"]::down-arrow {
            image: url(:/icons/arrow-up.svg);
        }

        /* Выпадающий список методов внутри popup. */
        /* QAbstractItemView - это внутренний список элементов combobox popup.
           Здесь задаем его фон, рамку, цвета текста и выделения. */
        QComboBox#methodComboBox QAbstractItemView {
            background: #ffffff;
            border: 1px solid #c8d8ea;
            outline: none;
            selection-background-color: #e6f1fb;
            selection-color: #173b5f;
            color: #1e3655;
            padding: 4px;
        }

        /* Настройка отдельных элементов списка внутри popup. */
        QComboBox#methodComboBox QAbstractItemView::item {
            min-height: 30px;
            padding: 4px 8px;
        }

        /* -----------------------------------------------------------------
           Главная кнопка запуска моделирования
           ----------------------------------------------------------------- */
        /* primaryButton - основной CTA-элемент интерфейса.
           Поэтому он заметно контрастнее остальных кнопок. */
        QPushButton#primaryButton {
            background: #2a6df4;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 15px;
            font-weight: 700;
            padding: 0 16px;
        }

        /* Наведение на кнопку слегка затемняет синий цвет. */
        QPushButton#primaryButton:hover {
            background: #1f61e4;
        }

        /* При нажатии кнопка затемняется еще сильнее. */
        QPushButton#primaryButton:pressed {
            background: #174fc4;
        }

        /* В disabled-состоянии кнопка становится бледной,
           чтобы визуально было понятно, что запуск сейчас недоступен. */
        QPushButton#primaryButton:disabled {
            background: #b2c8f2;
            color: #eaf1ff;
        }

        /* -----------------------------------------------------------------
           Вкладки правой панели результатов
           ----------------------------------------------------------------- */
        /* pane - область содержимого вкладок.
           Рамку здесь убираем, потому что карточки внутри вкладок
           уже имеют собственное оформление. */
        QTabWidget#resultsTabs::pane {
            border: none;
            top: -1px;
            background: transparent;
        }

        /* Сдвигаем bar немного вправо, чтобы вкладки не прилипали к левому краю. */
        QTabWidget#resultsTabs::tab-bar {
            left: 14px;
        }

        /* Базовый стиль вкладок:
           светлый фон, скругленные верхние углы, собственная рамка
           и заметная типографика. */
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

        /* При наведении вкладка становится чуть контрастнее. */
        QTabWidget#resultsTabs QTabBar::tab:hover {
            background: #e8f0f8;
            color: #284560;
        }

        /* Активная вкладка сливается по цвету с областью содержимого,
           поэтому кажется выбранной и "вынутой" вперед. */
        QTabWidget#resultsTabs QTabBar::tab:selected {
            background: #ffffff;
            color: #102033;
            margin-bottom: -1px;
        }

        /* -----------------------------------------------------------------
           Таблица выборки и ее заголовки
           ----------------------------------------------------------------- */
        /* QTableView - основная область таблицы:
           рамка, фон, шрифт, сетка, выделение и чередование строк. */
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

        /* Заголовки столбцов таблицы. */
        QHeaderView::section {
            background: #f3f7fb;
            color: #102033;
            border: none;
            border-bottom: 1px solid #d9e2ec;
            padding: 8px;
            font-size: 15px;
            font-weight: 700;
        }

        /* Верхний левый "угловой" элемент таблицы делаем согласованным
           с обычными секциями заголовков. */
        QTableCornerButton::section {
            background: #f3f7fb;
            border: none;
            border-bottom: 1px solid #d9e2ec;
        }

        /* -----------------------------------------------------------------
           Вертикальный scroll bar
           ----------------------------------------------------------------- */
        /* QScrollBar:vertical описывает всю область вертикальной прокрутки,
           то есть не только сам бегунок, но и фоновую дорожку, по которой
           этот бегунок перемещается.
           Здесь:
           - background задает мягкий светлый цвет дорожки;
           - width определяет общую толщину полосы прокрутки;
           - margin создает небольшой отступ от краев таблицы или другого
             прокручиваемого виджета, чтобы scrollbar не выглядел "влитым"
             прямо в рамку;
           - border-radius скругляет всю форму дорожки. */
        QScrollBar:vertical {
            background: #eef4fb;
            width: 12px;
            margin: 10px 2px 10px 2px;
            border-radius: 6px;
        }

        /* QScrollBar::handle:vertical - это подвижный бегунок,
           за который пользователь берется мышью и перемещает прокрутку.
           Важно понимать, что именно handle является главным интерактивным
           элементом scrollbar.
           Здесь:
           - background делаем чуть темнее дорожки, чтобы бегунок хорошо
             читался на ее фоне;
           - min-height не позволяет бегунку стать слишком маленьким при
             очень большом объеме содержимого;
           - border-radius сохраняет ту же округлую геометрию, что и у дорожки,
             чтобы весь элемент выглядел единым. */
        QScrollBar::handle:vertical {
            background: #c7d7e8;
            min-height: 36px;
            border-radius: 6px;
        }

        /* Состояние :hover срабатывает, когда курсор находится над бегунком.
           Легкое затемнение помогает показать пользователю, что элемент активен
           и готов к перетаскиванию. */
        QScrollBar::handle:vertical:hover {
            background: #b4c9de;
        }

        /* QScrollBar::add-line и QScrollBar::sub-line - это крайние кнопочные
           области стандартного scrollbar. В некоторых системных стилях Qt
           именно здесь могли бы появляться кнопки со стрелками вверх и вниз.
           В нашем интерфейсе такие кнопки визуально не нужны, поэтому:
           - height: 0px фактически убирает их из компоновки;
           - background: transparent не оставляет видимого следа;
           - border: none убирает лишние контуры.
           В результате остается только минималистичная дорожка и бегунок. */
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            background: transparent;
            border: none;
        }

        /* QScrollBar::add-page и QScrollBar::sub-page - это части дорожки,
           расположенные ниже и выше текущего бегунка.
           Они тоже участвуют в структуре scrollbar, но мы не хотим выделять их
           отдельными цветными блоками, поэтому оставляем их прозрачными.
           Так прокрутка выглядит легче и не отвлекает внимание от содержимого. */
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }

        /* -----------------------------------------------------------------
           Горизонтальный scroll bar
           ----------------------------------------------------------------- */
        /* QScrollBar:horizontal - это фоновая дорожка горизонтальной прокрутки.
           По смыслу она полностью аналогична вертикальной версии, но теперь:
           - height отвечает за толщину полосы;
           - margin задается с акцентом на отступы слева и справа,
             потому что скроллбар располагается вдоль нижней границы;
           - border-radius снова делает форму мягкой и визуально аккуратной. */
        QScrollBar:horizontal {
            background: #eef4fb;
            height: 12px;
            margin: 2px 10px 2px 10px;
            border-radius: 6px;
        }

        /* Горизонтальный бегунок работает по той же логике, что и вертикальный.
           Разница лишь в геометрии:
           - пользователь двигает его влево и вправо;
           - поэтому минимальный размер задается через min-width, а не min-height.
           Это важно для удобства захвата мышью при больших объемах данных. */
        QScrollBar::handle:horizontal {
            background: #c7d7e8;
            min-width: 36px;
            border-radius: 6px;
        }

        /* Подсвечиваем горизонтальный бегунок при наведении по той же причине:
           пользователь должен сразу видеть, что элемент интерактивен. */
        QScrollBar::handle:horizontal:hover {
            background: #b4c9de;
        }

        /* Это крайние кнопочные области горизонтального scrollbar,
           где стандартный стиль мог бы показать стрелки влево и вправо.
           Мы их также убираем, чтобы стиль интерфейса оставался единым
           и современным, без устаревших кнопочных элементов по краям. */
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0px;
            background: transparent;
            border: none;
        }

        /* Участки дорожки слева и справа от бегунка оставляем прозрачными,
           чтобы пользователь визуально концентрировался на самом handle,
           а не на второстепенных сегментах полосы прокрутки. */
        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");
}

// ----------------------------------------------------------------------------
// МЕТОД SetupSignals - ПОДКЛЮЧЕНИЕ СИГНАЛОВ И СЛОТОВ
// ----------------------------------------------------------------------------
// Здесь связываются пользовательские действия и завершение фонового расчета
// с соответствующими методами MainWindow.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::SetupSignals()
{
    // Нажатие основной кнопки запускает моделирование.
    // Это главный пользовательский сценарий работы окна.
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::RunSampling);
    // Смена метода обновляет видимость зависимых полей и теории.
    // Например:
    // - для метода обратной функции нужен шаг Δy;
    // - для метода исключения показывается строка про a, b, M и z<y.
    connect(methodComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        UpdateMethodSpecificInputsVisibility();
    });

    // После завершения фонового вычисления забираем результат
    // и обновляем все элементы интерфейса.
    // QFutureWatcher здесь выступает "мостом" между фоновым расчетом
    // и GUI-потоком главного окна.
    connect(simulationWatcher_, &QFutureWatcher<SamplingResult>::finished, this, [this]() {
        FinishSampling(simulationWatcher_->result());
    });
}

// ----------------------------------------------------------------------------
// МЕТОД ResetOutputs - СБРОС ВСЕХ ОТОБРАЖАЕМЫХ РЕЗУЛЬТАТОВ
// ----------------------------------------------------------------------------
// Используется перед новым запуском моделирования,
// чтобы очистить старые графики, сводку и таблицу.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ResetOutputs()
{
    // Сбрасываем текстовые поля сводки.
    // Тире используется как визуальный признак "значение пока не рассчитано".
    sampleCountValue_->setText("—");
    meanValue_->setText("—");
    varianceValue_->setText("—");
    medianValue_->setText("—");
    dnValue_->setText("—");
    knValue_->setText("—");
    criticalValue_->setText("—");
    hypothesisValue_->setText("—");
    acceptanceValue_->setText("—");
    // Снимаем индивидуальный цвет гипотезы,
    // чтобы после сброса она не оставалась "зеленой" или "красной".
    hypothesisValue_->setStyleSheet("");

    // Очищаем все графики.
    // После этого вкладки остаются на месте,
    // но внутри них нет серий данных.
    histogramPlot_->Clear();
    cdfPlot_->Clear();
    samplePlot_->Clear();

    // Отключаем таблицу от предыдущего результата.
    // Модель остается той же, но теряет ссылку на старые данные.
    static_cast<SampleTableModel*>(samplesModel_)->SetResult(nullptr);

    // Сбрасываем внутреннее состояние окна.
    // hasResult_ показывает, что в интерфейсе больше нет актуального расчета,
    // а lastResult_ очищается до состояния "пустой результат".
    hasResult_ = false;
    lastResult_ = SamplingResult{};
}

// ----------------------------------------------------------------------------
// МЕТОД RunSampling - ЗАПУСК МОДЕЛИРОВАНИЯ ИЗ GUI
// ----------------------------------------------------------------------------
// Метод:
// - читает значения полей;
// - валидирует ввод;
// - переводит интерфейс в состояние "идет расчет";
// - запускает вычисление в фоновом потоке через QtConcurrent.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::RunSampling()
{
    // Не допускаем повторного запуска, пока предыдущий расчет еще выполняется.
    // Это защищает от двойного клика по кнопке и параллельного старта двух задач.
    if (isSamplingRunning_)
    {
        return;
    }

    // Перед новым расчетом убираем старые результаты.
    // Так пользователь сразу видит, что начинается новый цикл моделирования,
    // а не просто обновляется старая картинка.
    ResetOutputs();

    // Проверяем корректность пользовательского ввода.
    // Если хотя бы один параметр некорректен,
    // в вычислительное ядро мы не идем.
    QString errorMessage;
    if (!ValidateInput(errorMessage))
    {
        // Показываем понятное сообщение и прекращаем запуск.
        ShowStatus(errorMessage, false);
        return;
    }

    // Собираем все параметры интерфейса в структуру SamplingOptions,
    // которую затем передадим вычислительному ядру.
    // Это удобно, потому что вычислительное ядро получает
    // один целостный объект настроек, а не множество разрозненных значений.
    SamplingOptions options;
    // По выбранному пункту combobox определяем,
    // какой именно метод моделирования нужен.
    options.method = methodComboBox_->currentIndex() == 0 ? SamplingMethod::InverseFunction
                                                          : SamplingMethod::Rejection;
    // Размер выборки n.
    options.sampleSize = static_cast<std::size_t>(sampleSizeEdit_->value());
    // Число интервалов гистограммы.
    options.histogramBins = static_cast<std::size_t>(binsEdit_->value());
    // Шаг интегрирования Δy.
    options.integrationStep = stepEdit_->value();
    // Уровень значимости α.
    options.alpha = alphaEdit_->value();

    // Переводим интерфейс в состояние активного расчета.
    // Это блокирует редактирование полей и меняет текст кнопки.
    ApplySamplingRunningState(true);
    // Сразу сообщаем пользователю, что расчет стартовал.
    ShowStatus("Идёт моделирование. После расчёта будут построены графики, таблица и статистика.", true);

    // Запускаем вычисление в фоне, чтобы не блокировать GUI-поток.
    // Лямбда просто передает структуру options в вычислительное ядро.
    // Когда задача завершится, QFutureWatcher вызовет FinishSampling(...).
    simulationWatcher_->setFuture(QtConcurrent::run([options]() { return ::RunSampling(options); }));
}

// ----------------------------------------------------------------------------
// МЕТОД ValidateInput - ПРОВЕРКА КОРРЕКТНОСТИ ВХОДНЫХ ДАННЫХ
// ----------------------------------------------------------------------------
// Если находится ошибка, метод записывает понятное сообщение в errorMessage
// и возвращает false.
//
// Принимает:
// - errorMessage: строку, в которую при ошибке записывается пояснение.
//
// Возвращает:
// - true, если все поля ввода корректны;
// - false, если найдена ошибка.
// ----------------------------------------------------------------------------
bool MainWindow::ValidateInput(QString& errorMessage) const
{
    // Размер выборки должен быть строго положительным,
    // иначе моделировать просто нечего.
    if (sampleSizeEdit_->value() <= 0)
    {
        errorMessage = "Размер выборки должен быть больше нуля.";
        return false;
    }

    // Гистограмма должна содержать хотя бы один интервал,
    // иначе ее невозможно построить.
    if (binsEdit_->value() <= 0)
    {
        errorMessage = "Число интервалов гистограммы должно быть больше нуля.";
        return false;
    }

    // Шаг Δy требуется только для метода обратной функции.
    // Для метода исключения этот параметр не участвует в вычислениях.
    if (methodComboBox_->currentIndex() == 0 && !(stepEdit_->value() > 0.0))
    {
        errorMessage = "Шаг Δy должен быть больше нуля.";
        return false;
    }

    // Уровень значимости должен лежать строго между 0 и 1,
    // иначе критерий Колмогорова теряет смысл.
    if (!(alphaEdit_->value() > 0.0 && alphaEdit_->value() < 1.0))
    {
        errorMessage = "Уровень значимости α должен принадлежать интервалу (0, 1).";
        return false;
    }

    // Если до этого места дошли без ошибок,
    // значит все основные входные параметры корректны.
    return true;
}

// ----------------------------------------------------------------------------
// МЕТОД UpdateMethodSpecificInputsVisibility - ПЕРЕКЛЮЧЕНИЕ ПОЛЕЙ ПО МЕТОДУ
// ----------------------------------------------------------------------------
// Метод обратной функции требует поле Δy,
// а метод исключения - строку теории про a, b, M и критерий принятия.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::UpdateMethodSpecificInputsVisibility()
{
    // currentIndex == 0 соответствует методу обратной функции.
    const bool isInverseMethod = methodComboBox_ != nullptr && methodComboBox_->currentIndex() == 0;
    // Шаг Δy показывается только для метода обратной функции.
    const bool showStep = isInverseMethod;
    // Во время активного расчета даже видимое поле должно быть недоступно.
    const bool enableStep = showStep && !isSamplingRunning_;
    // Теоретическая строка про метод исключения показывается,
    // только если выбран именно этот метод.
    const bool showRejectionTheory = !isInverseMethod;

    if (stepLabel_ != nullptr)
    {
        // stepLabel_ - это отдельная текстовая подпись "Шаг Δy" в форме.
        // Ее нужно скрывать отдельно, потому что QFormLayout хранит подпись
        // и поле как два независимых элемента строки.
        stepLabel_->setVisible(showStep);
    }

    if (stepFieldContainer_ != nullptr)
    {
        // stepFieldContainer_ - это внешний контейнер строки шага,
        // внутри которого лежат:
        // - само поле ввода stepEdit_;
        // - кнопки "+" и "-".
        //
        // Поэтому через него удобно управлять всей правой частью строки целиком.
        stepFieldContainer_->setVisible(showStep);
        // Во время активного расчета поле должно быть видно,
        // но недоступно для редактирования, если метод его использует.
        stepFieldContainer_->setEnabled(enableStep);
    }

    if (stepEdit_ != nullptr)
    {
        // stepEdit_ - это уже сам внутренний QDoubleSpinBox.
        // Мы дополнительно синхронизируем и его состояние тоже,
        // чтобы не получилось ситуации, когда контейнер и вложенное поле
        // "думают" по-разному о своей видимости или доступности.
        stepEdit_->setVisible(showStep);
        stepEdit_->setEnabled(enableStep);
    }

    if (rejectionTheoryLabel_ != nullptr)
    {
        // Левая подпись строки теории,
        // относящейся только к методу исключения.
        rejectionTheoryLabel_->setVisible(showRejectionTheory);
    }

    if (rejectionTheoryValueLabel_ != nullptr)
    {
        // Правая часть той же строки:
        // формула с a, b, M и критерием принятия.
        // Ее тоже нужно скрывать отдельно, потому что она является
        // самостоятельным QLabel в сетке блока теории.
        rejectionTheoryValueLabel_->setVisible(showRejectionTheory);
    }
}

// ----------------------------------------------------------------------------
// МЕТОД ApplySamplingRunningState - РЕЖИМ "РАСЧЕТ ИДЕТ / РАСЧЕТ ЗАВЕРШЕН"
// ----------------------------------------------------------------------------
// Пока моделирование идет, поля ввода и кнопка запуска блокируются,
// чтобы пользователь не менял параметры во время фонового вычисления.
//
// Принимает:
// - running: признак того, должен ли интерфейс перейти в режим активного расчета.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ApplySamplingRunningState(bool running)
{
    // Сохраняем текущее состояние расчета внутри окна.
    isSamplingRunning_ = running;

    // Во время расчета запрещаем менять метод,
    // чтобы интерфейс и вычислительная задача не расходились.
    methodComboBox_->setEnabled(!running);
    // Блокируем основные числовые поля,
    // чтобы пользователь не менял параметры "на лету".
    SetFieldContainerEnabled(sampleSizeEdit_, !running);
    SetFieldContainerEnabled(binsEdit_, !running);
    SetFieldContainerEnabled(alphaEdit_, !running);
    // Поле шага Δy и строка теории зависят от метода,
    // поэтому обновляем их отдельным методом.
    UpdateMethodSpecificInputsVisibility();

    // Главную кнопку тоже блокируем на время расчета
    // и меняем ее подпись, чтобы пользователь видел состояние окна.
    runButton_->setEnabled(!running);
    runButton_->setText(running ? "Идёт моделирование..." : "Выполнить моделирование");
}

// ----------------------------------------------------------------------------
// МЕТОД FinishSampling - ЗАВЕРШЕНИЕ ОБРАБОТКИ РЕЗУЛЬТАТА
// ----------------------------------------------------------------------------
// Здесь результат из вычислительного ядра либо превращается в сообщение
// об ошибке, либо раскладывается по таблице, сводке и графикам.
//
// Принимает:
// - result: структура результата, возвращенная вычислительным ядром.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::FinishSampling(const SamplingResult& result)
{
    // Независимо от исхода расчета снимаем режим "идет моделирование":
    // разблокируем поля и возвращаем обычную подпись кнопки.
    ApplySamplingRunningState(false);

    // Если вычислительное ядро вернуло ошибку,
    // просто показываем ее пользователю и прекращаем обновление GUI.
    if (!result.success)
    {
        ShowStatus(QString::fromStdString(result.message), false);
        return;
    }

    // Сохраняем полный результат внутри окна,
    // чтобы затем таблица и графики могли работать с актуальными данными.
    lastResult_ = result;
    // Фиксируем, что теперь у окна есть валидный расчет.
    hasResult_ = true;

    // Последовательно обновляем все представления результата.
    PopulateTable(result);
    UpdateSummary(result);
    UpdatePlots(result);

    // Финальный статус берется из сообщения вычислительного ядра.
    QString statusText = QString::fromStdString(result.message);
    ShowStatus(statusText, true);
}

// Передает последний рассчитанный результат в модель таблицы.
//
// Принимает:
// - result: рассчитанный результат моделирования.
//
// Возвращает:
// - ничего.
void MainWindow::PopulateTable(const SamplingResult& result)
{
    // Формально параметр result сюда приходит для единообразия интерфейса,
    // но сама таблица всегда привязывается к lastResult_,
    // который уже сохранен в FinishSampling(...).
    Q_UNUSED(result);
    // Передаем модели ссылку на актуальный последний результат,
    // после чего таблица сама отобразит его строки.
    static_cast<SampleTableModel*>(samplesModel_)->SetResult(&lastResult_);
}

// Заполняет сводный блок итоговыми статистиками и решением по H0.
//
// Принимает:
// - result: рассчитанный результат моделирования.
//
// Возвращает:
// - ничего.
void MainWindow::UpdateSummary(const SamplingResult& result)
{
    // Размер сформированной выборки.
    sampleCountValue_->setText(QString::number(result.samples.size()));
    // Выборочное среднее.
    meanValue_->setText(FormatNumber(result.empiricalStatistics.mean));
    // Выборочная дисперсия.
    varianceValue_->setText(FormatNumber(result.empiricalStatistics.variance));
    // Выборочная медиана.
    medianValue_->setText(FormatNumber(result.empiricalStatistics.median));
    // Статистика D_n критерия Колмогорова.
    dnValue_->setText(FormatNumber(result.kolmogorov.dn));
    // Статистика K_n.
    knValue_->setText(FormatNumber(result.kolmogorov.kn));
    // Критическое значение K_(1-alpha).
    criticalValue_->setText(FormatNumber(result.kolmogorov.criticalValue));
    // Формируем человекочитаемый вывод по нулевой гипотезе.
    hypothesisValue_->setText(result.kolmogorov.rejectNullHypothesis ? "H₀ отвергается"
                                                                     : "H₀ не отвергается");
    // Цветом дополнительно подчеркиваем итог критерия:
    // красный - отвергается, зеленый - не отвергается.
    hypothesisValue_->setStyleSheet(result.kolmogorov.rejectNullHypothesis
                                        ? "color: #9b1c1c; font-weight: 700;"
                                        : "color: #1d5a32; font-weight: 700;");

    // Доля принятия имеет смысл только для метода исключения.
    // Для метода обратной функции в поле выводим тире.
    if (result.acceptanceRate >= 0.0)
    {
        acceptanceValue_->setText(FormatNumber(result.acceptanceRate));
    }
    else
    {
        acceptanceValue_->setText("—");
    }
}

// Обновляет три вкладки графиков по новому результату моделирования.
//
// Принимает:
// - result: рассчитанный результат моделирования.
//
// Возвращает:
// - ничего.
void MainWindow::UpdatePlots(const SamplingResult& result)
{
    // На вкладке гистограммы рисуем:
    // - контур эмпирической гистограммы по выборке;
    // - теоретическую плотность f(x)=2x.
    histogramPlot_->SetSeries(
        {
            {"Гистограмма выборки", BuildHistogramOutline(result.histogram), QColor("#2563eb"), 2.4, Qt::SolidLine},
            {"Теоретическая плотность  f(x)=2x", BuildDensityCurvePoints(), QColor("#dc2626"), 2.0, Qt::DashLine},
        });

    // На вкладке функции распределения рисуем:
    // - эмпирическую ступенчатую функцию Fэмп(x);
    // - теоретическую функцию F(x)=x^2.
    cdfPlot_->SetSeries(
        {
            {"Эмпирическая функция  F(x)", BuildEmpiricalDistributionPoints(result.sortedSamples), QColor("#1d4ed8"), 2.4, Qt::SolidLine},
            {"Теоретическая функция  F(x)=x²", BuildTheoreticalDistributionPoints(), QColor("#059669"), 2.0, Qt::DashLine},
        });

    // На вкладке последовательности показываем сами сгенерированные значения
    // в порядке их появления в выборке.
    samplePlot_->SetSeries(
        {
            {QString("%1: x_i").arg(MethodTitle(result.method)),
             BuildSampleSequence(result.samples),
             QColor("#7c3aed"),
             1.8,
             Qt::SolidLine},
        });
}

// Показывает пользователю статусную плашку успеха или ошибки.
//
// Принимает:
// - text: текст сообщения для пользователя;
// - success: признак успешного или ошибочного статуса.
//
// Возвращает:
// - ничего.
void MainWindow::ShowStatus(const QString& text, bool success)
{
    // Обновляем текст статусной плашки.
    statusBadge_->setText(text);

    if (success)
    {
        // Зеленое оформление для успешных и информационных сообщений.
        statusBadge_->setStyleSheet(
            "border: 1px solid #94d7aa; background: #eaf8ef; color: #1d5a32; "
            "border-radius: 12px; padding: 10px 12px;");
    }
    else
    {
        // Красное оформление для ошибок и проблемных состояний.
        statusBadge_->setStyleSheet(
            "border: 1px solid #efb7bf; background: #fff1f3; color: #7b2636; "
            "border-radius: 12px; padding: 10px 12px;");
    }
}

// Делегирует форматирование чисел локальной вспомогательной функции файла.
//
// Принимает:
// - value: число, которое нужно вывести в GUI.
//
// Возвращает:
// - строку с отформатированным числом.
QString MainWindow::FormatNumber(double value)
{
    // Вся числовая форматировка централизована в одной helper-функции,
    // поэтому здесь просто передаем ей управление.
    return FormatNumericValue(value);
}
