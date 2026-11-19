# Implementation boundary

The firmware LLD currently defines interfaces and ownership contracts only.
Concrete domain services, platform adapters and role composition are intentionally
not implemented in this stage.

Keep implementation classes in focused translation units when implementation
begins. Do not add Arduino or ESP-IDF dependencies to this shared interface layer.
