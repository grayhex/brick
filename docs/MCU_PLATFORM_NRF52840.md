# Brick V1 — выбор MCU и программной платформы (ADR-037)

**Дата:** 2026-10-20. **Статус: MCU ARCHITECTURE SELECTED.** Это решение для разработки прошивки Brick V1, а **не** квалификация компонента для выпуска 100–500 изделий. PCB, LCD, аккумулятор и электрические параметры ещё не утверждены.

## Решение

**Nordic nRF52840** — единственный целевой SoC для V1. **ARM Cortex-M4F 64 МГц, 1 МиБ внутренней Flash, 256 КиБ RAM, BLE, USB 2.0 Full Speed (12 Мбит/с), SPI/QSPI, I²S, PWM, SAADC, SWD.** C11 переносимое ядро `firmware/` остаётся без Nordic/Zephyr API.

**Ориентир RF-модуля для PCB**: **Raytac MDBT50Q-P1MV2** (nRF52840 Rev2, интегрированная PCB-антенна, модуль примерно 10.5×15.5×2.0 мм). Поддерживаемые функциональные линии, размеры, supply, 32.768kHz clock, RF keep-out и сертификационный статус проверяются по актуальному даташиту модуля до схемы. На одной микросхеме — BLE + USB, отдельный BT-чип не нужен. Альтернативный модуль (например, Raytac `MDBT50Q-P1MEN`, nRF52840 Rev3 с изменениями защиты доступа) **не считается автоматически взаимозаменяемым** и должен быть квалифицирован отдельно.

**Разделение статусов**:
- `nRF52840` — **SELECTED / архитектура утверждена для развития ПО**.
- `MDBT50Q-P1MV2` — **reference module / CANDIDATE для закупки**, пока не получены MOQ, samples, актуальная ревизия, PCB fit, антенна и тесты.
- `nRF52840 DK` (PCA10056) — **выбранная стандартная плата для разработки**. Её встроенная QSPI NOR = 64 Мбит / 8 МиБ. Это не плата Brick и не подтверждение энергопотребления финального устройства.
- `Sharp LS032B7DD02` + подсветка — **не утверждены**. Риск `fSCS` 1–10 Гц и читаемости панели справа остаётся критическим.

### Альтернативы, которые не берём в V1

| MCU | Причина |
| --- | --- |
| Nordic nRF5340 | Сильнее, но добавляет отдельное network-core firmware, многокомпонентное DFU и сложнее жизненный цикл без потребности в LE Audio |
| Nordic nRF54L15 | Современная маломощная архитектура, но нет встроенного USB периферийного интерфейса для прямого USB-C DFU |
| ESP32-S3 | Два ядра и больше RAM; BLE+USB есть, но Wi-Fi и графическая мощность не нужны моноблочной монохромной игрушке. Потребление всей системы надо было бы отдельно сравнить |
| STM32WB55 / BLE-модули | Возможны, но не дают преимуществ относительно цельной реализации BLE+USB+SDK на Nordic для текущего набора |
| FPGA | Не нужна для C-реализации классических игр, усложнит Bluetooth, питание и загрузчик |

## Зафиксированный SDK / среда

- **nRF Connect SDK (NCS) v3.4.1 LTS**, на базе Zephyr. Фиксировать ровно тег `v3.4.1` в west workspace / сборочном контейнере и lock в инструкции/CI; **не собирать against latest/main**. Ветка 3.4 LTS имеет длительный период исправлений; обновление тега — отдельный проверяемый PR.
- **Ограничение долгосрочной поддержки:** Nordic прямо указывает, что **ветка NCS 3.4 LTS — последняя с поддержкой nRF52**; поддержку планируют вести **5 лет**, с исправлениями важных ошибок и безопасности, но не с добавлением новых функций для nRF52. Это осознанный выбор ради готовых BLE+USB+QSPI сегодня; если понадобится новая аппаратная платформа в будущем, переносимое ядро C11 сохранит возможность миграции. Не переносить проект на NCS `main`/3.5+ автоматически.
- Встроенный стек Bluetooth LE от Nordic/Zephyr, **без Classic Bluetooth и LE Audio**.
- **MCUboot** как один подписывающий и проверяющий загрузчик, **MCUmgr/SMP** как протокол управления образами.
- **USB 2.0 CDC ACM** для пользовательского DFU (в приложении) и **независимого serial recovery в MCUboot** при испорченной прошивке.
- **QSPI NOR в финальной плате — архитектурное требование для независимого второго образа (slot1)**: минимум 8 МиБ, ориентир 16 МиБ, конкретный MPN после электрического/энергетического анализа; на DK уже 8 МиБ.
- **BLE GATT** для телефона (настройки/статистика), **не для звука**. BLE OTA — *следующий этап, не требование первой версии*.
- **Детерминированное ядро** (60 Гц) остаётся в `firmware/` и продолжает собираться/тестироваться без Zephyr. Портирование — отдельная обёртка в `platform/nrf52840/`.

См. [дизайн DFU/загрузчика](FIRMWARE_UPDATE_NRF52840.md), [папка реализации на DK](../platform/nrf52840/README.md), [реальные требования к PCB](../hardware/pcb/requirements.md).

## Системная архитектура

```text
nRF52840 (single-core, Zephyr / NCS v3.4.1 LTS)
 ├── MCUboot (signed image verification, trial/revert, USB CDC recovery)
 └── Application
      ├── Brick C11 game core at fixed 60Hz ── platform-neutral bk_hal_t
      ├── GPIO buttons + debouncing
      ├── LCD frame sender (display-specific, max_fps)
      ├── frontlight PWM + sleep/power state machine
      ├── beeper/audio output (+ driver for selected amp/speaker)
      ├── nonvolatile Brick profile BKSV A/B + Zephyr settings for BLE bonding
      ├── BLE GATT for phone config/sync
      ├── USB CDC ACM / MCUmgr (firmware image upload)
      └── watchdog, reset reason, firmware/HW version and diagnostics
                    │
              external QSPI NOR: slot1 for signed DFU + optional assets
```

Игровое ядро **не знает**, что оно работает под Zephyr, а система **не знает** правил Tetris/Snake. Zephyr управляет таймерами, событиями, электроникой, BLE/USB и sleep. `bk_console_tick` вызывается в назначенной задаче фиксированного шага 60 Гц; USB/BLE не должны блокировать игровой цикл; FPS реального LCD отдельно ограничивается адаптером.

**Память**: уже имеющиеся два 1-битных framebuffer на 336×536 занимают ~45 КиБ RAM; это допустимое начало в 256 КиБ, но добавить память Zephyr, стеки, BLE/USB, игры и графику, измерить результат полной линковки. `.data=.bss=0` в статической библиотеке **не** означает нулевую RAM прошивки.

## Критерии окончательной аппаратной квалификации

Проверить на DK с MCUboot + BLE + USB, а затем на PCB: настоящую скорость игр и стек/flash размер, допустимость аппаратного `double/fmod` с теми же conformance traces, батарейный профиль 20–30 ч (не гарантируется MCU), запуск после выключения и восстановления питания, сигналы QSPI, RF дальность в корпусе, FPC/подсветку и стабильность USB-C.

**Не менять MCU без нового ADR**, но отдельную панель, аудиоусилитель и форму батареи разрешено выбирать и заменять независимо, пока держим границы HAL.

## Официальные источники

- nRF52840: https://www.nordicsemi.com/products/nrf52840
- Raytac PCB antenna module: https://www.raytac.com/product/ins.php?index_id=47
- Nordic DK: https://www.nordicsemi.com/Products/Development-hardware/nrf52840-dk
- DK внешняя Flash 8 MiB: https://docs.nordicsemi.com/r/bundle/ug_nrf52840_dk/page/ug/dk/hw_external_memory.html
- NCS v3.4.1 LTS: https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/releases_and_maturity/releases/release-notes-3.4.1.html
- Zephyr board target: https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dk/doc/index.html
