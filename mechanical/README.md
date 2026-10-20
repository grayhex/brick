# Brick mechanical CAD

**Рабочий источник геометрии — параметрический:** [params.json](params.json) (числа с пометкой статуса) и код на CadQuery в [src](src/README.md). [drawings](drawings/README.md) — реальные чертежи, разрезы, проверки посадки; [exports](exports/README.md) — проверенные STEP/STL. `inbox/buildcad-r0.1/` — архив присланных бинарных STL с известными замечаниями ([аудит](../docs/MECHANICAL_MOCKUP_REVIEW.md)); не изменять и не использовать как production master.

CAD brief: [CAD_HANDOFF_PROMPT](../docs/CAD_HANDOFF_PROMPT.md), порядок работы: [CAD_WORKFLOW](../docs/CAD_WORKFLOW.md). Согласовано: приблизительный корпус 175×80, 4 раздельные жёлтые круглые кнопки направлений, 1 гладкая Action, MODE/SELECT/START под LCD, единственный speaker outlet спереди по центру внизу, USB-C на нижнем торце; три finishes на одной базе. Пока не определены толщина, батарея, LCD/FPC, PCB outline, допуски и материал оснастки.

## Что есть сегодня (ревизия `M0.2-draft`)

| Что | Где | Статус |
| --- | --- | --- |
| Параметры R0.2 (габарит, окно LCD, 8 кнопок, решётка динамика, USB-C) | [params.json](params.json) | **PROPOSED**: рабочие значения из CAD-задания, на физическом образце не проверялись |
| Проверка чисел без CAD-ядра (зазоры, границы, требования PRD) | [src/check_params.py](src/check_params.py) | работает на чистом Python; ловит противоречия в числах, но не доказывает эргономику |
| Генератор макета: два корпуса, восемь кнопок отдельными телами, заглушка LCD, шов посередине толщины | [src/build.py](src/build.py) и модули рядом | **макет для печати и проверки эргономики**, не литьё; внутренности (батарея, плата, антенна), крепёж и боковые органы **не моделируются**: решений нет |
| Отчёт проверки | [validation.md](validation.md) | создаётся `build.py`; проверяет генератор, а не посадку в руке |
| Проверка, что проверки умеют краснеть | [src/test_check_params.py](src/test_check_params.py), [src/test_validation.py](src/test_validation.py) | восемь намеренных поломок чисел и восемь поломок модели, каждая обязана быть поймана |

## Воспроизвести

```sh
python3 -m venv .venv && . .venv/bin/activate
pip install -r mechanical/requirements.txt            # CadQuery 2.8.0 (OpenCascade), около 300 МБ с зависимостями
python3 mechanical/src/check_params.py                # быстро, без CadQuery
python3 mechanical/src/test_check_params.py           # проверка проверки чисел
python3 mechanical/src/build.py --report mechanical/validation.md   # STEP/STL в mechanical/build/ (в git не хранится)
python3 mechanical/src/build.py --thickness 22        # вариант 22 мм (по умолчанию 20 мм)
python3 mechanical/src/test_validation.py             # проверка проверки
```

Результат открывается в бесплатном [FreeCAD](https://www.freecad.org/) (STEP) или в слайсере (STL). Экспорт в git кладут только после проверки на физическом образце, вместе с `M_REV` и SHA исходника (см. [exports](exports/README.md)); CI сохраняет свежий экспорт как артефакт.

## Чего нет, и это нормально

Нет посадки реального дисплея, FPC и подсветки, аккумулятора, платы, антенны, винтов и бобышек, выключателя и колеса громкости, допусков литья. Они появятся после образцов и решений владельца D-1…D-6 ([ROADMAP](../docs/ROADMAP.md)). Не выдавать красивую картинку за доказательство посадки.
