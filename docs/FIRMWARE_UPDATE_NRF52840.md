# Brick V1 — безопасная загрузка, USB-C DFU, восстановление (ADR-038/039)

**Дата:** 2026-10-20. **Статус:** архитектура и правила обязательны для первой аппаратной версии, **реализация/сборка/отладка ещё не выполнены**. Программная платформа: [nRF52840 + NCS 3.4.1 LTS](MCU_PLATFORM_NRF52840.md).

## Требования без компромиссов

1. Владелец обновляет ПО с компьютера **через нижний USB-C** без J-Link / вскрытия; новые игры поступают внутри прошивки, не как независимые исполняемые плагины.
2. Ошибка подписи, потеря питания или сбой новой версии **не должны превращать устройство в «кирпич»**. Должна остаться штатная версия (rollback) или независимый USB recovery загрузчика.
3. **MCUboot** проверяет цифровую подпись образа до запуска. Для V1 — ECDSA P-256 с публичным ключом в bootloader, приватный production signing key **не размещать в GitHub/GitHub Actions secrets без отдельного продуманного доступа**; хранить offline/в защищённом signing-сервисе. Раздельные development и production ключи. В github не хранить реальные приватные ключи.
4. **Два независимых слота** приложения: `slot0` — выполняемый образ во внутренней Flash; `slot1` — второй образ во внешней QSPI NOR. Режим `swap with revert` / подтверждение рабочей версии; никаких `overwrite-only` режимов без rollback. MCUboot должен поддерживать фактические geometry/erase blocks обоих устройств.
5. После первого запуска новой версии: self-test (состояние хранилища, версия HW и прошивки, инициализация основных сервисов, базовая диагностика) → **только после этого** пометить образ `confirmed`. Сбой до подтверждения → boot revert. Нельзя подтверждать сразу на старте до проверок.
6. **Два независимых USB-пути**: `MCUmgr/SMP over USB CDC ACM` в живом приложении для обычного обновления и `MCUboot USB CDC ACM serial recovery` для случая неисправного/отсутствующего приложения. Не путать с устаревшим `nrfutil nrf5sdk-tools` (другой bootloader/протокол).
7. В аппаратном прототипе предусмотреть **SWD/RESET test pads** для заводской первоначальной прошивки и аварийного сервиса. В серийном изделии пользователь должен восстанавливаться через USB-C; SWD не является единственным recovery.
8. Для входа в recovery без отверстия Reset: назначить **удержание MODE при включении** (комбинация и конкретный GPIO уточняются и проверяются в MCUboot). Bootloader должен читать кнопку *до* запуска приложения. Физическое включение/USB-зарядка должны быть возможны по схеме питания. Никаких акустических/новых внешних отверстий.
9. Все изменения Flash partitions после продаж опасны и требуют отдельной процедуры миграции, подписанного пакета совместимости, тестов и разрешения. **Не менять адреса разделов между сборками на основе автоматического случайного layout.**
10. Пользовательские настройки `BKSV`, рекорды и BLE pairing переживают обновление (отдельный data partition), прошивки несовместимых ревизий HW должны отвергаться до активации.

## Логика обновления

```text
App v1 (slot0) ── USB CDC ACM / MCUmgr SMP ── image chunks ── external slot1
                                     ↓
                         image header + hash + signature verify
                                     ↓
                             request TEST upgrade
                                     ↓
                      reboot → MCUboot swap/test → App v2
                                     ↓
                     startup checks → image confirmed
                              ┌──────┴──────┐
                         success         crashes/no confirm
                              │               │
                         keep v2         revert to v1
```

**Serial recovery:** выключить Brick → удерживать MODE → включить питание и подключить USB → MCUboot входит в recovery независимо от приложения → signed DFU + проверка → нормальный запуск. Реальную комбинацию/power-rail/USB enumeration доказать тестом, это контракт, не реализованная функция.

Сервисная USB-утилита — отдельный desktop CLI для пользователя (Windows/macOS/Linux) поверх совместимого MCUmgr клиента; в будущем можно добавить GUI. Ни одного изображения в `.bin` нельзя считать релизным без подписи/хеша и проверки версии.

## Предварительная схема Flash (НЕ заморожена в DTS)

Внутренняя NOR nRF52840 = **1 МиБ**. Возможный *пример* для оценки бюджета:
- `0x00000 … 0x1FFFF` — загрузчик **128 KiB** (MCUboot+USB recovery; размер проверить на линковке).
- `0x20000 … 0xDFFFF` — `slot0` приложение **768 KiB** (образ должен уместиться вместе с заголовком/метаданными/trailer).
- `0xE0000 … 0xFFFFF` — user settings/storage **128 KiB**, независимо от слота прошивки.

QSPI NOR в продукте ориентировочно **16 МиБ**, минимум 8 МиБ:
- `slot1` на внешней Flash **не меньше размера slot0**, отдельный раздел;
- оставшаяся NOR — ресурсы/ассеты при необходимости, причём **не** XIP-код на nRF52840 по умолчанию.

**Это инженерная оценка, НЕ действующий overlay/partition map.** И Bootloader с USB recovery + QSPI может не уложиться в 128 KiB: первый обязательный этап — собрать **все** sysbuild images, проверить linker map, erase sector alignment, `BOOT_MAX_IMG_SECTORS`, swap-revert и сохраняющиеся данные. Не фиксировать адреса до полного прототипа. Для первых испытаний допускается нативный layout DK, но финальный layout нужно тестировать до выпуска и больше не менять.

### Хранилище настроек

Сохранять действующий `BKSV` формат профиля из `firmware/core/save.c` (2 слота A/B с sequence+CRC). На плате оборачивать в энергонезависимый backend Zephyr **NVS или ZMS**, выделить место для его собственных метаданных, erase/wear management. BLE bonds/settings — через Zephyr settings subsystem, **логически отдельно** от `BKSV`, без конфликта ключей. Не писать Flash каждый 60-герцовый игровой такт; буферизация и проверка поведения при выключении.

### Безопасность и предупреждения

- MCUboot **проверяет подпись приложения**, но не делает аппаратную защиту от физического вскрытия абсолютной. Выбор модулей Rev2/Rev3 и включение APPROTECT рассмотреть отдельно; SWD service и массовая прошивка требуют управляемой политики.
- Не выпускать прошивку с development signing key; ключи подписания не должны встраиваться в CI без политики секретов.
- Протокол MCUmgr settings имеет потенциально опасные операции. На пользовательском USB/BLE разрешить только требуемые группы/операции, закрыть shell/filesystem/debug на релизных сборках. BLE write требует auth/bonding; USB update требует подписи и совместимости.
- User-facing bootloader нельзя автоматически перезаписать через стандартное приложение. Для V1 предполагаем **стабильный неудаляемый загрузчик**, его обновление — отдельная сервисная процедура и архитектурное решение (не обещать обновление самого MCUboot через USB).
- Power-loss tests обязательны: прервать передачу; прервать swap; выключить в первый boot до confirm; залить неверную подпись/версию/аппаратную ревизию; потерять внешнюю QSPI; storage migration; соединение прерывается посреди transfer.

## Порядок реализации агентом

**M1.** DK nRF52840, **NCS 3.4.1**, `west build --sysbuild`: собрать/прошить чистое приложение MCUboot, видеть versions/flash map.

**M2.** Добавить USB CDC ACM, **MCUboot serial recovery** (надёжный recovery без работающего приложения); проверить цепочку повреждённой прошивки/USB update на DK. Только основной **nRF USB разъём платы**, не J-Link USB.

**M3.** Включить QSPI NOR DK **8 МиБ** и `slot1` с явным Devicetree/Partition Manager по текущему NCS; создать подписанный образ, загрузить, swap/test/confirm/revert. Записать итоговый layout и сделать тест на отключение питания.

**M4.** Создать `platform/nrf52840/` CMake/Zephyr glue к `libbrickcore`, не трогая `firmware/`, сохранить существующие gcc/clang/conformance тесты. Один экран/порт HAL можно заглушить до выбора настоящего LCD.

**M5.** USB MCUmgr в приложении + CLI upload/verify/report, валидировать ключи, версии, совместимость, восстановление данных. Затем BLE GATT for settings, OTA по BLE как **опция более позднего этапа**.

**M6.** Снять реальную RAM/Flash/CPU/ток + USB/BLE совместную нагрузку, оформить release and manufacturing test procedures.

## Официальная документация

- Zephyr DFU: https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html
- Zephyr MCUmgr/SMP: https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
- USB DFU + serial recovery: https://academy.nordicsemi.com/courses/nrf-connect-sdk-intermediate/lessons/lesson-9-bootloaders-and-dfu-fota/topic/exercise-4-dfu-over-usb/
- External Flash MCUboot DFU: https://academy.nordicsemi.com/?nds_version_content=nrf-connect-sdk-intermediate-lesson-9-exercise-3-dfu-with-external-flash-v3-3-0
- Stable DTS partitions + external QSPI: https://github.com/nrfconnect/sdk-nrf/blob/main/doc/nrf/app_dev/bootloaders_dfu/mcuboot_nsib/bootloader_partitioning.rst
- Swap/confirmation in Zephyr SMP sample: https://docs.zephyrproject.org/latest/samples/subsys/mgmt/mcumgr/smp_svr/README.html
- USB docs include both MCUboot and running application: https://academy.nordicsemi.com/courses/nrf-connect-sdk-intermediate/lessons/lesson-9-bootloaders-and-dfu-fota/topic/exercise-4-dfu-over-usb/
- Zephyr settings/NVS/ZMS: https://docs.zephyrproject.org/latest/services/storage/settings/index.html
