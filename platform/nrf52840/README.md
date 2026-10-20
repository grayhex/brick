# Nordic nRF52840 — платформа Brick V1

**Выбранная целевая архитектура:** nRF52840 + NCS **v3.4.1 LTS** / Zephyr, **MCUboot signed image + revert**, USB-C CDC ACM DFU + independent bootloader recovery, BLE GATT для телефона, внешняя QSPI NOR для слота обновлений.

Эта папка — **board-specific реализация системной платформы**. Сейчас в ней **собирающаяся** sysbuild-конфигурация для nRF52840 DK (MCUboot + приложение), но она **ещё не прошита и не проверена на плате**: рабочими MCUboot, USB recovery, swap и откат считать нельзя ([NRF52840_BRINGUP.md](../../docs/NRF52840_BRINGUP.md), [HIL_TESTPLAN.md](tests/HIL_TESTPLAN.md)). `libbrickcore` сюда ещё не подключён.

- [**Состояние сборки, размеры, что не проверено**](../../docs/NRF52840_BRINGUP.md) · [план испытаний на DK](tests/HIL_TESTPLAN.md) · [обновление по USB для пользователя](../../docs/USB_UPDATE.md)
- [Выбор MCU](../../docs/MCU_PLATFORM_NRF52840.md)
- [Загрузчик/обновление/восстановление](../../docs/FIRMWARE_UPDATE_NRF52840.md)
- [Аппаратная плата и сигналы](../../hardware/pcb/requirements.md)
- [Текущее переносимое C11 ядро](../../firmware/) и [его тесты](../../tests/)
- [Черновик BLE GATT](../../companion/gatt-draft.md) — **предложение**, а не утверждённый протокол

## Разработка без собственной PCB

**Заказать/использовать Nordic nRF52840 DK** (PCA10056) — официально поддерживается в Zephyr как `nrf52840dk/nrf52840`; есть USB-порт самого чипа (отдельный от интерфейсного J-Link), кнопки и внешняя QSPI NOR **8 МиБ**. Внешняя Flash DK позволяет отладить двухслотовое обновление, не покупая самодельную плату. Комплекс Bluetooth/USB+MCUboot на DK проверяется *перед* кастомным PCB.

**SDK lock**: `nrfconnect/sdk-nrf` тег `v3.4.1`. Не использовать `main`, nightly или произвольный `latest`. `west`, toolchain и CMake из соответствующего NCS. Изолировать SDK: `nrf/` workspace **вне** репозитория Brick, а `platform/nrf52840/` — application tree, подхватывающий общую C11 библиотеку.

## Сборка

```sh
# из каталога platform/nrf52840
scripts/setup_ncs.sh ~/ncs-ws             # один раз: NCS v3.4.1 вне репозитория (≈ 3 ГБ), venv с west
export NCS_DIR=~/ncs-ws/ncs               # BRICK_TOOLCHAIN=nrfutil (официальный набор NCS) или gnuarmemb
scripts/build.sh --variant release        # MCUboot + приложение, dev-ключ создаётся один раз вне репозитория
python scripts/verify_build.py build/release --ncs $NCS_DIR --key ~/.config/brick/dev-keys/brick-dev-ecdsa-p256.pem
west flash -d build/release               # на DK; НЕ ПРОВЕРЕНО (нет платы в среде разработки)
```

Версии всех проектов SDK — в [`ncs-v3.4.1.sources.txt`](ncs-v3.4.1.sources.txt). Измерения сделаны GNU Arm Embedded 13.2.1, а не официальным набором NCS (R-19).

## Что здесь есть

```text
platform/nrf52840/
├── CMakeLists.txt, VERSION, Kconfig   # приложение (имя sysbuild-образа = имя папки: nrf52840)
├── prj.conf                           # MCUmgr img+os по USB CDC ACM, QSPI NOR, watchdog; без shell/fs/stat
├── sysbuild.conf, sysbuild.cmake      # MCUboot swap-using-move, ECDSA P-256; сборка без dev-ключа отклоняется
├── sysbuild/mcuboot.{conf,overlay}    # USB CDC ACM serial recovery, кнопка SW1, QSPI, BOOT_MAX_IMG_SECTORS=192
├── dts/brick_dk_memory_map.dtsi       # ЕДИНАЯ карта памяти DK для приложения и MCUboot (стендовая, не заморожена)
├── boards/nrf52840dk_nrf52840.overlay
├── src/main.c                         # версия, состояние образа, самопроверка, подтверждение, watchdog
├── variants/{noconfirm,crash,hang}.conf   # ТЕСТОВЫЕ варианты для испытаний отката, в выпуск не идут
├── scripts/                           # setup_ncs, gen_dev_key, build, verify_build, env
├── tests/HIL_TESTPLAN.md              # испытания на DK: 17 пунктов, результаты НЕ ВЫПОЛНЕНЫ
└── ncs-v3.4.1.sources.txt             # названные версии (коммиты) SDK
```

Приложение сознательно **пустое**: не игра, не `bk_hal_t`. Подключение `libbrickcore` — отдельный PR (таймбаза 60 Гц, кнопки, заглушки дисплея и звука, хранилище A/B, энтропия), без изменений `firmware/`.

Ещё не создано: `drivers/` (HAL), собственная плата Brick H0.1, factory-тесты, BLE.

## Границы реализации

- `firmware/` — строго C11 и аппаратно-независим. Ни одной прямой зависимости на Zephyr, Nordic, MCUboot, USB или BLE.
- `platform/nrf52840/` — системные службы, Zephyr, драйверы и перенаправление `bk_hal_t`. Один 60Гц game loop; команды BLE/USB не тормозят его.
- Первые игры по PRD: **Tetris, Racing, Snake и Tank**; без дополнительных вариантов на старте.
- Покупка MCU-модуля для 100–500 шт. и маркировка его QUALIFIED требуют datasheet/PCB RF review/availability/current tests. Reference module `Raytac MDBT50Q-P1MV2` не считается автоматически сертифицированным готовым устройством.
- Нельзя фиксировать LCD, flash offsets или pin mapping по рендерам корпуса. На DK известны QSPI линии P0.17/P0.19–P0.23, для собственной PCB — спроектировать и документировать overlay и keep-out.
- Отдельно тестировать успешный/провальный USB DFU, активацию/revert, работу при обрыве внешней Flash, сохранение рекордов и BLE pairing.

## Достаточный результат первого PR

1. Воспроизводимая **реальная** sysbuild конфигурация на `nrf52840dk/nrf52840` и прошивка на плате.
2. На USB CDC ACM доступны состояние прошивки и **независимый режим serial recovery MCUboot**; удержание реальной кнопки на DK и в будущей PCB.
3. Подписанные firmware images, корректный `slot1` в QSPI, два прошедших сценария: update/confirm и update/revert.
4. Измеренная `ROM/RAM` footprint, полный `zephyr.dts` map для app и MCUboot, результаты тестов питания и известные ограничения.
5. Отдельный PR для подключения существующего `libbrickcore`, сохранение всех host/CI тестов.

Если на DK нет доступа к настоящему LCD или аудиотракту, оставлять их **mock HAL**, но не выдавать за готовые драйверы.

**Состояние по пунктам** (2026-10-20): (1) конфигурация собирается воспроизводимо, **прошивки на плате нет**; (2) serial recovery настроен в сборке, **не проверен**; (3) образы подписаны и проверяются `imgtool`, слот `slot1` в QSPI размечен, сценарии update/confirm и update/revert **не выполнялись** (собраны образы `release`, `noconfirm`, `crash`, `hang`); (4) размеры и карта памяти измерены на слинкованных образах, **тестов питания нет**; (5) отдельный PR.
