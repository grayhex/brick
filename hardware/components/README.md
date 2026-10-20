# Components: BOM и AVL

- [candidates.csv](candidates.csv) — **кандидаты и вопросы по ним**, собранные из [исследования железа](../../docs/HARDWARE_RESEARCH.md) и [кандидатов дисплеев](../../docs/DISPLAY_CANDIDATES.md). Это не закупочный список: ни одна деталь не утверждена, образцы не заказывались, измерений нет. Цен, наличия и сроков в файле нет намеренно: они меняются и появятся только из настоящих котировок.
- [bom.template.csv](bom.template.csv) — только заголовки таблицы, **не закупочный заказ**.
- [approved-parts.template.csv](approved-parts.template.csv) — только заголовки списка допустимых альтернатив/квалификации.

Статусы: `CANDIDATE`, `SAMPLED`, `QUALIFIED`, `REJECTED`; для функции, где кандидата пока нет, — `NO_CANDIDATE`. Для `QUALIFIED`: MPN/footprint, даташит, подтверждённая ревизия, поставщик/SKU, MOQ, цены на 100/500, lead time, lifecycle, фото/sample evidence, альтернатива или её отсутствие. LiPo — габарит, защита/NTC, транспортные документы. **PCB-derived BOM + AVL должны совпасть с конкретным hardware revision.** Freeze для партии — в `releases/`. Существующие предварительные модели дисплея/MCU не утверждены.

Правило перехода: новая деталь получает строку в `candidates.csv` со ссылкой на даташит, затем образец и измерение, затем ADR в [DECISIONS.md](../../docs/DECISIONS.md), и только потом строку в BOM платы ([требования к плате](../pcb/requirements.md)).
