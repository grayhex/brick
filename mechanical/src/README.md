# Parametric CAD — source of truth

Здесь CAD-агент создаст редактируемые исходники (CadQuery/build123d Python или FreeCAD `.FCStd` + скрипт), таблицу параметров и воспроизводимую генерацию деталей/сборки. **Не создавать единственную master-модель в STL**. Для R0.2 следовать [CAD brief](../../docs/CAD_HANDOFF_PROMPT.md); экспортировать в `../exports/`, изображения и размеры в `../drawings/`. Существующие `../inbox/` STL — лишь проверяемый вход.
