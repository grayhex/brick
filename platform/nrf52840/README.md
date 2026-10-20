# Nordic nRF52840 — платформа Brick V1

**Выбранная целевая архитектура:** nRF52840 + NCS **v3.4.1 LTS** / Zephyr, **MCUboot signed image + revert**, USB-C CDC ACM DFU + independent bootloader recovery, BLE GATT для телефона, внешняя QSPI NOR для слота обновлений.

Эта папка — **точка входа для новой board-specific реализации**, и пока содержит описание архитектуры/требований, а **не рабочий нативный ELF**, bootloader, проверенный `prj.conf` или DeviceTree.

- [Выбор MCU](../../docs/MCU_PLATFORM_NRF52840.md)
- [Загрузчик/обновление/восстановление](../../docs/FIRMWARE_UPDATE_NRF52840.md)
- [Аппаратная плата и сигналы](../../hardware/pcb/requirements.md)
- [Текущее переносимое C11 ядро](../../firmware/) и [его тесты](../../tests/)
- [Черновик BLE GATT](../../companion/gatt-draft.md) — **предложение**, а не утверждённый протокол

## Разработка без собственной PCB

**Заказать/использовать Nordic nRF52840 DK** (PCA10056) — официально поддерживается в Zephyr как `nrf52840dk/nrf52840`; есть USB-порт самого чипа (отдельный от интерфейсного J-Link), кнопки и внешняя QSPI NOR **8 МиБ**. Внешняя Flash DK позволяет отладить двухслотовое обновление, не покупая самодельную плату. Комплекс Bluetooth/USB+MCUboot на DK проверяется *перед* кастомным PCB.

**SDK lock**: `nrfconnect/sdk-nrf` тег `v3.4.1`. Не использовать `main`, nightly или произвольный `latest`. `west`, toolchain и CMake из соответствующего NCS. Изолировать SDK: `nrf/` workspace **вне** репозитория Brick, а `platform/nrf52840/` — application tree, подхватывающий общую C11 библиотеку.

Типовой старт, после установки NCS (команды должны быть проверены в реальной среде):
```sh
# Из каталога NCS workspace, там где есть nrf/, zephyr/:
west build -b nrf52840dk/nrf52840 --sysbuild zephyr/samples/subsys/mgmt/mcumgr/smp_svr \
  -- -DEXTRA_CONF_FILE="cdc.conf" -DEXTRA_DTC_OVERLAY_FILE="usb.overlay"
west flash
```
Это **официальный upstream sample**, не финальное приложение Brick и не включённая в репозиторий сборка. Для USB DFU использовать именно USB, подключённый к nRF52840, не виртуальный порт отладчика.

## Предлагаемая структура реализации

```text
platform/nrf52840/
├── README.md                   # здесь, контракт MCU/platform
├── CMakeLists.txt              # добавить при реализации
├── sysbuild.conf               # MCUboot, подпись, layout — только проверенные настройки
├── prj.conf                    # Zephyr settings, MCUmgr, USB, BLE, timers
├── app/                        # main.c: Zephyr init, game loop и системные службы
├── drivers/                    # bk_hal_t: gpio, LCD, audio, flash, light, battery
├── boards/                     # nrf52840dk overlay и будущий brick H0.1 custom board
├── sysbuild/mcuboot/           # independent CDC ACM serial recovery
└── tests/                      # smoke/hardware-in-loop, подписанный DFU, power-loss
```

**Никаких готовых `prj.conf`, overlay и linker scripts ещё нет.** Не создавать фальшивые «работающие» конфигурации: агент должен сначала проверить SDK v3.4.1 и реальный nRF52840 DK, затем создать их с результатами сборки/реальной прошивки.

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
