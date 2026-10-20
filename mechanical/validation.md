# Проверка параметрической модели корпуса (генерируется)

Файл создаёт `mechanical/src/build.py`; **не править вручную**. Он проверяет генератор и согласованность чисел, а **не посадку
в руке, не дисплей и не детали**: все размеры из `params.json` — предложенные (PROPOSED), корпус не проверялся на физическом образце.

* Ревизия параметров: `M0.2-draft`, SHA-256 `params.json`: `3991a8486f20b44c6b4e25e80d8c35c897db18b5940fef48ce7cf36d0c17d5c5`
* Толщина: 20 мм, шов посередине толщины (допущение для макета)
* Инструменты: CadQuery 2.8.0, Python 3.13.16

| Проверка | Результат | Подробности |
| --- | --- | --- |
| parameter checks (check_params.py) | PASS | 39 of 39 passed |
| front_shell is a valid single solid with volume | PASS | volume 22775.6 mm3 |
| rear_shell is a valid single solid with volume | PASS | volume 32261.0 mm3 |
| function_key_mode is a valid single solid with volume | PASS | volume 166.2 mm3 |
| function_key_select is a valid single solid with volume | PASS | volume 166.2 mm3 |
| function_key_start is a valid single solid with volume | PASS | volume 166.2 mm3 |
| direction_key_up is a valid single solid with volume | PASS | volume 300.4 mm3 |
| direction_key_left is a valid single solid with volume | PASS | volume 300.4 mm3 |
| direction_key_right is a valid single solid with volume | PASS | volume 300.4 mm3 |
| direction_key_down is a valid single solid with volume | PASS | volume 300.4 mm3 |
| action_key is a valid single solid with volume | PASS | volume 1575.2 mm3 |
| display_placeholder is a valid single solid with volume | PASS | volume 16795.5 mm3 |
| envelope is 80 x 175 x 20 mm | PASS | x 0.000..80.000, y 0.000..175.000, z 0.000..20.000 |
| no two parts intersect | PASS | largest overlap 0.000000 mm3 |
| the LCD window is open | PASS | centre (40.0, 50.0) |
| function_key_mode sits in its hole | PASS | (25, 101), diameter 6.5 |
| function_key_select sits in its hole | PASS | (40, 101), diameter 6.5 |
| function_key_start sits in its hole | PASS | (55, 101), diameter 6.5 |
| direction_key_up sits in its hole | PASS | (24, 119), diameter 9.0 |
| direction_key_left sits in its hole | PASS | (12.5, 130.5), diameter 9.0 |
| direction_key_right sits in its hole | PASS | (35.5, 130.5), diameter 9.0 |
| direction_key_down sits in its hole | PASS | (24, 142), diameter 9.0 |
| action_key sits in its hole | PASS | (59, 130.5), diameter 21.5 |
| the USB-C cut-out is open on the bottom edge | PASS | x=40 |
| the front speaker grille is perforated and the webs between its holes are solid | PASS | 11 x 4 holes of 1.2 mm, pitch 2.0 mm (placeholder pattern) |
| the front plate has only the LCD window, eight key holes and the grille holes | PASS | plate volume 15589.45 mm3, expected 15589.45 mm3 |
| the rear plate is closed (no holes, no speaker outlet) | PASS | plate volume 25074.84 mm3 of 25074.84 mm3 |
| the side, top and bottom walls have no opening besides the USB-C cut-out | PASS | USB-C cut-out 9.5 x 3.5 mm only |

## Не моделируется (решения не приняты)

* battery size, NTC/PCM and swelling allowance
* PCB outline and antenna keep-out (depends on the chosen BLE module)
* LCD/FPC exit and frontlight stack
* fasteners, bosses and seam
* moulding or printing process, tolerances
* microSD slot (no cut-out in R0.2)

Единственные источники истины остаются `params.json` и код в `mechanical/src/`; экспорт (STEP/STL) создаётся этой же командой и в git не хранится.
