from enum import Enum

ANSI_ESCAPE = '\x1B'

class Color(Enum):
    RESET = '[0m'
    RED = '[31m'
    YELLOW = '[33m'
    CYAN = '[36m'
    GRAY = '[90m'
    MAGENTA = '[95m'

    def __call__(self, content: str):
        return self.wrap(content)

    def wrap(self, content: str) -> str:
        return f'{ANSI_ESCAPE}{self.value}{content}{ANSI_ESCAPE}{Color.RESET.value}'
