"""現行の配置検査・回収計画・デコーダで共有する既存ELF symbol読取り。
旧integration検査から処理を移したもの。ビルド設定や実機実行許可は生成しない。
"""
import re
import struct
from pathlib import Path
from check_boot_plan import elf_inventory

ELF_SECTION_SIZE = 64
ELF_SYMBOL_SIZE = 24
ELF_SYMTAB = 2
ELF_STRTAB = 3
ELF_NOBITS = 8


def require(condition, message):
    if not condition:
        raise ValueError(message)


def elf_symbols(path):
    # 固定buildのELF64 LEを対象とし、未知の拡張形式は受け入れない。
    elf_inventory(path)
    data = Path(path).read_bytes()
    require(data[4:6] == b'\x02\x01', 'AArch64 ELF64 LE required')
    offset = struct.unpack_from('<Q', data, 40)[0]
    size, count = struct.unpack_from('<HH', data, 58)
    require(size == ELF_SECTION_SIZE and count > 0 and offset + size * count <= len(data), 'invalid ELF sections')
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, offset + size * index) for index in range(count)]
    symbols = {}
    for section in sections:
        if section[1] != ELF_SYMTAB:
            continue
        begin, length, link, stride = section[4], section[5], section[6], section[9]
        require(stride == ELF_SYMBOL_SIZE and length % stride == 0 and begin + length <= len(data) and link < count, 'invalid symbol table')
        strings = sections[link]
        require(strings[1] == ELF_STRTAB and strings[4] + strings[5] <= len(data), 'invalid symbol strings')
        names = data[strings[4]:strings[4] + strings[5]]
        for cursor in range(begin, begin + length, stride):
            name, info, other, index, address, extent = struct.unpack_from('<IBBHQQ', data, cursor)
            if not index or not name:
                continue
            require(name < len(names) and b'\0' in names[name:], 'unterminated symbol name')
            label = names[name:names.index(b'\0', name)].decode('utf-8')
            symbols[label] = dict(address=address, size=extent, section=index)
    require(symbols, 'ELF symbols are required to verify dispatcher and Early TAs')
    return data, sections, symbols


def symbol_payload(parsed, name):
    data, sections, symbols = parsed
    require(name in symbols, 'missing linked symbol: ' + name)
    symbol = symbols[name]
    require(symbol['section'] < len(sections), 'symbol outside section table')
    section = sections[symbol['section']]
    displacement = symbol['address'] - section[3]
    require(section[1] != ELF_NOBITS and 0 <= displacement and symbol['size'] > 0 and displacement + symbol['size'] <= section[5], 'symbol escapes file-backed section')
    start = section[4] + displacement
    require(start + symbol['size'] <= len(data), 'symbol payload truncated')
    return data[start:start + symbol['size']]


def read_config(path):
    return dict(re.findall(r'^([A-Za-z0-9_]+)=(.*)$', path.read_text(), re.M))
