# Лабораторная работа 1

Qt/C++ приложение для моделирования непрерывной случайной величины с плотностью

`f(x) = 2x`, `0 <= x <= 1`.

В проекте реализованы два метода из теории лабораторной работы:
- метод обратной функции;
- метод исключения.

Приложение строит:
- гистограмму выборки и теоретическую плотность;
- эмпирическую и теоретическую функции распределения;
- график выборки;
- таблицу значений;
- выборочные характеристики и проверку по критерию Колмогорова.

## Теория варианта

- плотность: `f(x) = 2x`, `0 <= x <= 1`;
- функция распределения: `F(x) = x^2`, `0 <= x <= 1`;
- математическое ожидание: `2/3`;
- дисперсия: `1/18`;
- медиана: `1/sqrt(2)`;
- мода: `1`.

Для метода исключения используются:
- `a = 0`;
- `b = 1`;
- `M = 2`;
- критерий принятия: `z < y`.

## Возможности интерфейса

- выбор метода моделирования;
- настройка размера выборки, числа интервалов гистограммы, шага `Δy`, уровня значимости `α`;
- интерактивные графики с масштабированием и перемещением;
- hover-подсказки со значениями на графиках;
- автоматическое сравнение с теоретическим законом;
- вывод `D_n`, `K_n`, критического значения и решения по гипотезе `H0`.

## Сборка на macOS

Требования:
- macOS на Apple Silicon;
- CMake;
- Xcode Command Line Tools;
- Qt 6 с модулями `Widgets`, `Charts`, `Concurrent`;
- `macdeployqt`.

Сборка:

```bash
cmake --preset default-debug
cmake --build build/default-debug --parallel 6
```

Запуск:

```bash
open "build/default-debug/ModelingMethodsLab1.app"
```

Или напрямую:

```bash
"build/default-debug/ModelingMethodsLab1.app/Contents/MacOS/ModelingMethodsLab1"
```

## Структура проекта

- [main.cpp](main.cpp) — запуск приложения и настройка путей Qt;
- [include/MainWindow.h](include/MainWindow.h), [src/MainWindow.cpp](src/MainWindow.cpp) — интерфейс и логика окна;
- [include/PlotChartWidget.h](include/PlotChartWidget.h), [src/PlotChartWidget.cpp](src/PlotChartWidget.cpp) — интерактивные графики;
- [include/SamplingEngine.h](include/SamplingEngine.h), [src/SamplingEngine.cpp](src/SamplingEngine.cpp) — генерация выборки и статистика;
- [include/VariantDistribution.h](include/VariantDistribution.h) — параметры распределения варианта;
- [ЛР_1.pdf](ЛР_1.pdf) — исходное задание.
