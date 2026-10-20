# Сторонние материалы и происхождение

**Статус:** черновик аудита (2026-10-19), **не юридическое заключение**. Перед публичным или коммерческим выпуском прошивки и ассетов проверить каждый пункт (AGENTS.md: «Check source/third-party licensing before copying assets or code»).

## 1. Правила игр: n0Live/BrickGame (MIT)

Правила Tetris в `firmware/games/tetris/` и Snake в `firmware/games/snake/` — перенос на C правил эталона [ApolloBrickGames](https://github.com/grayhex/ApolloBrickGames), который, в свою очередь, написан заново на TypeScript по открытому симулятору [n0Live/BrickGame](https://github.com/n0Live/BrickGame) (Java). Код донора не копировался, но правила и таблицы унаследованы, поэтому сохранено его уведомление:

```
MIT License

Copyright (c) 2017 Egor Krasnopolin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 2. ApolloBrickGames (эталон и источник части данных)

* Репозиторий принадлежит тому же владельцу. **Файла LICENSE в нём нет** (аудит коммита `abd7c3426f28222b54db2fe280597d7578e7191c`): условия повторного использования для третьих лиц не заданы.
* Из него перенесены как **данные**: таблица фигур и поворотов, таблица таймера падения B0202, константы очков и скоростей, ГСЧ mulberry32, формулы раскладки превью — исходные файлы перечислены в [GAME_TETRIS.md](docs/GAME_TETRIS.md).
* Скопированы как **ассеты** из `src/brick/menuArt.ts`: крупные цифры 4×7 и буквы A–Z 4×7, мелкие цифры 3×5, иллюстрация Tetris 10×5. Эталон сам отмечает, что цифра «2» и часть иллюстраций нарисованы **по фотографии экрана оригинала**, остальные — «в том же стиле». Права на воспроизведение этих рисунков не установлены (риск R-3 в [DECISIONS.md](docs/DECISIONS.md)): до публичного/коммерческого выпуска проверить или перерисовать.
* Тесты правил `tests/unit/test_tetris_rules.c` портированы со сценариев `tests/brick/games/tetris.test.ts` эталона (те же числа и условия).
* Snake (`firmware/games/snake/`): перенесены как **данные и правила** класс `SnakeGame` из `src/brick/games/library/arcade.ts`, общая часть `LibraryGame` (`library/common.ts`: жизни, победа, разбор клавиш), десять периодов шага E88 из `src/brick/e88.ts`, клетки старта и константы (13 плодов, +10, +100, четыре жизни). Спрайты Snake не копировались: миниатюра меню нарисована для этого проекта (два кадра 10×5), цифры и буквы — те же ассеты `menuArt.ts` с оговоркой R-3. Тесты `tests/unit/test_snake_rules.c` — собственные.

## 3. BrickEmuPy (CC0-1.0)

Таблица времени падения B0202 (`150, 120, 100, 82, 60, 41, 35, 31, 27, 25` × 2048 / 690 000 с) и десять периодов шага Snake (E88 «8 in 1», игра 5: 332,358 … 37,038 мс) получены в эталоне из исполнения ROM в [azya52/BrickEmuPy](https://github.com/azya52/BrickEmuPy) (репозиторий CC0-1.0). В этом репозитории **нет ни ROM, ни кода эмулятора**, только эти числа; мы ROM не исполняли и значения не перепроверяли. CC0 эмулятора не является разрешением на распространение чужих ROM.

## 4. Инструменты разработки (не поставляются)

* [esbuild](https://github.com/evanw/esbuild) (MIT) — сборка экспортёра фикстур, устанавливается через `npm ci` в `tools/conformance`, в репозиторий не входит (`node_modules` игнорируется).
* Исходники эталона читаются экспортёром из отдельного checkout, в репозиторий не копируются.

## 5. Написано в этом репозитории

Шрифт 3×5 (`firmware/core/font.c`), 7-сегментные цифры, значки звука и паузы, звуковые заглушки, раскладка панели, конечный автомат консоли, формат профиля — авторские. Внешних файлов шрифтов, звуков и изображений нет.
