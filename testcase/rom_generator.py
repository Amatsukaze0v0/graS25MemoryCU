import random

FILENAME = "testRom.txt"
NUM_WORDS = 32       # 你想生成多少个 32-bit ROM word
HEX_RATIO = 0.5       # 输出为 hex 格式的比例（其余为十进制）


def generate_rom_line():
    value = random.randint(0, 0xFFFFFFFF)
    if random.random() < HEX_RATIO:
        return hex(value)
    else:
        return str(value)


def main():
    try:
        with open(FILENAME, 'w', encoding='utf-8') as f:
            for _ in range(NUM_WORDS):
                f.write(generate_rom_line() + "\n")
        print(f"成功生成 ROM 内容文件 '{FILENAME}'，共 {NUM_WORDS} 个 32-bit 值。")
    except IOError as e:
        print(f"生成失败: {e}")


if __name__ == '__main__':
    main()
