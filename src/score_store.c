#include "score_store.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"
#include "pico/error.h"
#include "pico/platform.h"

#define SCORE_MAGIC 0x534E414Bu // "SNAK"
#define SCORE_VERSION 1u
#define SCORE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t score;
    uint32_t checksum;
} ScoreRecord;

static uint32_t record_checksum(const ScoreRecord *record) {
    return record->magic ^ record->version ^ record->score ^ 0xC35A91E7u;
}

uint32_t score_store_load(void) {
    const ScoreRecord *record =
        (const ScoreRecord *)(XIP_BASE + SCORE_FLASH_OFFSET);
    if (record->magic != SCORE_MAGIC || record->version != SCORE_VERSION ||
        record->checksum != record_checksum(record)) {
        return 0;
    }
    return record->score;
}

static void __not_in_flash_func(write_score_callback)(void *param) {
    const ScoreRecord *record = (const ScoreRecord *)param;
    uint32_t page[FLASH_PAGE_SIZE / sizeof(uint32_t)] __attribute__((aligned(4)));
    for (uint32_t i = 0; i < FLASH_PAGE_SIZE / sizeof(uint32_t); ++i) {
        page[i] = 0xFFFFFFFFu;
    }
    const uint8_t *source = (const uint8_t *)record;
    uint8_t *destination = (uint8_t *)page;
    for (uint32_t i = 0; i < sizeof *record; ++i) {
        destination[i] = source[i];
    }

    flash_range_erase(SCORE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SCORE_FLASH_OFFSET, (const uint8_t *)page,
                        FLASH_PAGE_SIZE);
}

bool score_store_save(uint32_t score) {
    if (score <= score_store_load()) return true;

    ScoreRecord record = {
        .magic = SCORE_MAGIC,
        .version = SCORE_VERSION,
        .score = score,
        .checksum = 0
    };
    record.checksum = record_checksum(&record);

    return flash_safe_execute(write_score_callback, &record, 1000) == PICO_OK;
}
