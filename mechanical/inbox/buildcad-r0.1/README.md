# Первый CAD-макет корпуса (buildcad.ai), входной материал

**Статус:** архив присланного владельцем макета, **не** исходник истины и не CAD-проект проекта. Размеры и раскладка — [docs/MECHANICAL_CONCEPT.md](../../../docs/MECHANICAL_CONCEPT.md) R0.2 и [CAD_HANDOFF_PROMPT.md](../../../docs/CAD_HANDOFF_PROMPT.md); разбор макета — [docs/MECHANICAL_MOCKUP_REVIEW.md](../../../docs/MECHANICAL_MOCKUP_REVIEW.md).

| | |
| --- | --- |
| Файл | `ecf3a131-untitled-part-stl.zip` (13 бинарных STL, 1 787 137 байт) |
| SHA-256 | `8da7e44690ef2d5924b395e40e5c9fc7e91aded519efb8e5b5e314f4cd5f5c20` |
| Источник | приложен владельцем в сессии 2026-10-19; редактируемая модель на buildcad.ai (`d_K5ObpFwD2Kez`) открывается только после входа, содержимое страницы без входа недоступно |
| Формат | только STL в миллиметрах (по размерам деталей); **нет** STEP, параметров, имён элементов, цвета, материалов |
| Части | `front`, `rear`, `lens`, `action`, `wheel`, `btn_mode`, `btn_select`, `btn_start`, `dir_up`, `dir_left`, `dir_right`, `dir_down`, `power` |

Детали разложены на условной «плите», а не в сборочном положении. Лицевая оболочка лежит длинной стороной вдоль X; инструмент разбора поворачивает её в систему R0.2 (проверка допущения — окно LCD должно оказаться в верхней половине).

## Воспроизведение

```sh
pip install numpy Pillow
python3 tools/cad_review/stl_review.py mechanical/inbox/buildcad-r0.1/ecf3a131-untitled-part-stl.zip \
  --verify mechanical/inbox/buildcad-r0.1/measurements.json              # сверка с сохранёнными измерениями
python3 tools/cad_review/stl_review.py mechanical/inbox/buildcad-r0.1/ecf3a131-untitled-part-stl.zip \
  --json /tmp/m.json --png-dir /tmp/png                                  # свежие измерения и рисунки
```

`measurements.json` — результат инструмента на этом архиве (контрольная точка CI). Если архив заменён новой ревизией, обновите JSON и [обзор](../../../docs/MECHANICAL_MOCKUP_REVIEW.md) вместе.

Архив распаковывать не нужно: инструмент читает файлы `untitled-part-*.stl` прямо из ZIP.
