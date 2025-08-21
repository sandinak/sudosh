---
type: "manual"
---

- never allow sudosh to call sudo directly as this creates a fork bomb
- always update documentation
- always require that any feature added has a unit test and regression tests
- always test any new feature for viability using the testing suite
- always make sure we're not introducing any security issues via any feature
- always make sure the features work in sudosh and sudo compatibility mode
