# PCB fabrication — controlled exports

**Заводских файлов пока нет**: создаются из проверенной KiCad-платы в отдельном каталоге `<H_REV>/`. В комплект конкретной ревизии входят Gerber, сверловка, контур/слои/толщина и покрытие, fab notes, PDF схемы, pick-and-place для обеих сторон, сборочные чертежи, stencil, BOM/AVL и DNP, DRC/DFM-отчёты, source commit SHA, версия KiCad и sha256 пакета. Не создавать фиктивные Gerber; утверждённый комплект записать в [releases](../../releases/README.md).
