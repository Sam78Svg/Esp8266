import os
import re
import urllib.request
import struct

# ============================================================
# SETTINGS
# ============================================================

URL = (
    "https://cdn.jsdelivr.net/gh/"
    "hagezi/dns-blocklists@latest/"
    "adblock/light.txt"
)

DATA_DIR = "data"

SOURCE_FILE = "hagezi_temp.txt"

BLOOM_FILE = os.path.join(
    DATA_DIR,
    "bloom.bin"
)

HASH_FILE = os.path.join(
    DATA_DIR,
    "hashes.bin"
)

BLOOM_BITS = 1_048_576
BLOOM_BYTES = BLOOM_BITS // 8
BLOOM_HASHES = 4

os.makedirs(DATA_DIR, exist_ok=True)

# ============================================================
# FNV-1A 64
# ============================================================

def fnv1a64(text):

    h = 14695981039346656037

    for byte in text.encode("utf-8"):

        h ^= byte

        h = (
            h * 1099511628211
        ) & 0xFFFFFFFFFFFFFFFF

    return h


# ============================================================
# SECOND HASH
# MUST MATCH ESP8266 CODE
# ============================================================

def hash2(text):

    h = 0xcbf29ce484222325

    for i, byte in enumerate(
        text.encode("utf-8")
    ):

        h ^= (
            byte +
            0x9 +
            i
        )

        h = (
            h *
            0x100000001b3
        ) & 0xFFFFFFFFFFFFFFFF

        h ^= (
            h >> 29
        )

    return h


# ============================================================
# BLOOM POSITIONS
# ============================================================

def bloom_positions(text):

    h1 = fnv1a64(text)
    h2 = hash2(text)

    for i in range(BLOOM_HASHES):

        position = (
            h1 +
            i * h2
        ) % BLOOM_BITS

        yield position


# ============================================================
# DOWNLOAD
# ============================================================

print()
print("==========================================")
print("ESP8266 BLOCKLIST BUILDER")
print("==========================================")

print()
print("Downloading HaGeZi Multi LIGHT...")

urllib.request.urlretrieve(
    URL,
    SOURCE_FILE
)

print("Download complete.")


# ============================================================
# EXTRACT DOMAINS
# ============================================================

domains = set()

print()
print("Extracting domains...")

with open(
    SOURCE_FILE,
    "r",
    encoding="utf-8",
    errors="ignore"
) as f:

    for line in f:

        line = line.strip()

        if not line:
            continue

        if line.startswith("!"):
            continue

        if not line.startswith("||"):
            continue

        value = line[2:]

        value = re.split(
            r"[\^/$*]",
            value
        )[0]

        value = value.strip().lower()

        if "." not in value:
            continue

        if " " in value:
            continue

        if "*" in value:
            continue

        if len(value) > 253:
            continue

        domains.add(value)


print(
    "Unique domains:",
    len(domains)
)


# ============================================================
# CREATE BLOOM FILTER
# ============================================================

print()
print("Creating Bloom filter...")

bloom = bytearray(
    BLOOM_BYTES
)

for domain in domains:

    for position in bloom_positions(
        domain
    ):

        byte_index = (
            position // 8
        )

        bit_index = (
            position % 8
        )

        bloom[byte_index] |= (
            1 << bit_index
        )


with open(
    BLOOM_FILE,
    "wb"
) as f:

    f.write(bloom)


print(
    "Bloom filter:",
    len(bloom),
    "bytes"
)


# ============================================================
# CREATE HASH DATABASE
# ============================================================

print()
print("Creating hash database...")

hashes = []

for domain in domains:

    h1 = fnv1a64(domain)
    h2 = hash2(domain)

    hashes.append(
        (h1, h2)
    )


hashes.sort()


with open(
    HASH_FILE,
    "wb"
) as f:

    for h1, h2 in hashes:

        f.write(
            struct.pack(
                "<QQ",
                h1,
                h2
            )
        )


print(
    "Hash entries:",
    len(hashes)
)

print(
    "Hash database:",
    len(hashes) * 16,
    "bytes"
)


# ============================================================
# DELETE TEMPORARY SOURCE
# ============================================================

try:

    os.remove(
        SOURCE_FILE
    )

    print(
        "Temporary source removed."
    )

except FileNotFoundError:

    pass


# ============================================================
# FINISHED
# ============================================================

print()
print("==========================================")
print("BUILD COMPLETE")
print("==========================================")

print()
print("Files created:")

print(
    "data/bloom.bin"
)

print(
    "data/hashes.bin"
)

print()
print("Now upload the filesystem again.")