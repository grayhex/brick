# Brick hardware — схемы и PCB

**Статус: исследования, не изготовленная плата.** [PCB](pcb/README.md) — редактируемый KiCad; [components](components/README.md) — BOM/AVL; [fabrication](fabrication/README.md) — экспорты для завода по конкретной ревизии.

До разводки: проверить кандидат Sharp 3.16″ по FPS, контрасту и ширине панели статуса, узнать FPC/напряжения frontlight; выбрать BLE MCU (nRF52840 пока лишь кандидат), аккумулятор/контроллер питания, динамик/усилитель, механику и USB-C. Обязательны SWD/test pads, ESD/CC/USB, NTC/защита LiPo, RF antenna keep-out и DFM.

Текущая информация: [hardware research](../docs/HARDWARE_RESEARCH.md), [display candidates](../docs/DISPLAY_CANDIDATES.md), [engineering checklist](../docs/RELEASE_COMPLETENESS_CHECKLIST.md). Не выдавать наличие папки за завершённую схему.
