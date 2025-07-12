import csv
import random

FILENAME = "requests.csv"
NUM_ROWS = 100 # Number of requests generated
ADDRESS_MAX = 32 # Max address of the ROM


def generate_random_row():

    # Type and Width
    req_type = random.choice(['R', 'W'])
    is_wide = random.choice(['T', 'F'])

    # Address to read, Hexadecimal or Decimal
    addr_val = random.randint(0, ADDRESS_MAX)
    if random.choice([True, False]):
        address = hex(addr_val)
    else:
        address = str(addr_val)

    # Generate User-id
    if random.random() < 0.1:  # 10% Chance to have a special user (0 or 255)
        user_val = random.choice([0, 255])
    else:
        user_val = random.randint(1, 254)

    if random.choice([True, False]):
        user = hex(user_val)
    else:
        user = str(user_val)

    data = ""
    if req_type == 'W':
        # 1 Byte write, no longer than 0xFF
        if is_wide == 'F':
            data_val = random.randint(0, 0xFF)
        # 4 Bytes write
        else:
            data_val = random.randint(0, 0xFFFFFFFF)

        # Random choose the type of data
        if random.choice([True, False]):
            data = hex(data_val)
        else:
            data = str(data_val)

    return [req_type, address, data, user, is_wide]


def main():
    header = ["Type", "Address", "Data", "User", "Wide"]

    try:
        with open(FILENAME, 'w', newline='', encoding='utf-8') as f:

            writer = csv.writer(f, delimiter=',', quotechar='"', quoting=csv.QUOTE_ALL)
            writer.writerow(header)

            for _ in range(NUM_ROWS):
                writer.writerow(generate_random_row())

        print(f"Success! Generated {NUM_ROWS} rows of requests to the file: '{FILENAME}'.")

    except IOError as e:
        print(f"Error: Failed to generate '{FILENAME}': {e}!")


if __name__ == '__main__':
    main()