# Components: BOM и AVL

- [bom.template.csv](bom.template.csv) — только заголовки таблицы, **не закупочный заказ**.
- [approved-parts.template.csv](approved-parts.template.csv) — только заголовки списка допустимых альтернатив/квалификации.

Статусы: `CANDIDATE`, `SAMPLED`, `QUALIFIED`, `REJECTED`. Для `QUALIFIED`: MPN/footprint, даташит, подтверждённая ревизия, поставщик/SKU, MOQ, цены на 100/500, lead time, lifecycle, фото/sample evidence, альтернатива или её отсутствие. LiPo — габарит, защита/NTC, транспортные документы. **PCB-derived BOM + AVL должны совпасть с конкретным hardware revision.** Freeze для партии — в `releases/`. Существующие предварительные модели дисплея/MCU не утверждены.
