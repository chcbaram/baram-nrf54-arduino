/*
 * bonding — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include "bonding.h"
#include "sd_event_pump.h"
#include <string.h>

extern "C" {
#include "nrf_soc.h"
}

/* 링커가 정해 주는 파티션. 앱 파티션 밖이다. */
extern uint32_t __bond_storage_start__;
extern uint32_t __bond_storage_size__;

#define BOND_MAGIC          (0x424F4E44UL)   /* "BOND" */
#define BOND_FLASH_TIMEOUT  (2000)

typedef struct {
  uint32_t       magic;
  uint8_t        role;
  uint8_t        sys_attr_len;
  uint16_t       _pad;
  bond_keys_t    keys;
  uint8_t        sys_attr[BOND_SYS_ATTR_MAX];
} bond_record_t;

static_assert(sizeof(bond_record_t) <= BOND_SLOT_SIZE,
              "bond record must fit one slot");
/* sd_flash_write 는 워드 단위라 슬롯이 4의 배수여야 한다. */
static_assert((BOND_SLOT_SIZE % 4) == 0, "slot must be word sized");

static uint32_t bond_base(void)
{
  return (uint32_t) &__bond_storage_start__;
}

static const bond_record_t *slot_at(uint8_t i)
{
  return (const bond_record_t *) (bond_base() + (uint32_t) i * BOND_SLOT_SIZE);
}

static bool slot_used(uint8_t i)
{
  return slot_at(i)->magic == BOND_MAGIC;
}

/* 슬롯 하나를 통째로 쓴다. RRAM 은 덮어쓸 수 있으므로 지울 필요가 없다. */
static bool slot_write(uint8_t i, const bond_record_t *rec)
{
  /*
   * ⚠ sd_flash_write 는 원본도 **워드 정렬**을 요구한다. 슬롯 전체를 한 번에
   *   쓰기 위해 정렬된 스택 버퍼로 복사한다 (256 B — 스레드 스택 안에서 안전).
   */
  uint32_t buf[BOND_SLOT_SIZE / 4];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, rec, sizeof(bond_record_t));

  return sdFlashWrite((uint32_t *) (bond_base() + (uint32_t) i * BOND_SLOT_SIZE),
                      buf, BOND_SLOT_SIZE / 4, BOND_FLASH_TIMEOUT);
}

static bool addr_equal(const ble_gap_addr_t *a, const ble_gap_addr_t *b)
{
  return (a->addr_type == b->addr_type) && (memcmp(a->addr, b->addr, 6) == 0);
}

/* 주소로 슬롯을 찾는다. 없으면 -1. */
static int8_t slot_find(uint8_t role, const ble_gap_addr_t *addr)
{
  for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
    if (!slot_used(i)) continue;

    const bond_record_t *r = slot_at(i);
    if (r->role != role) continue;

    if (addr_equal(&r->keys.peer_id.id_addr_info, addr)) return (int8_t) i;
  }

  /*
   * 주소로 못 찾았고 상대가 **resolvable private** 이면 저장된 IRK 로 풀어 본다.
   * 폰은 프라이버시 때문에 주소를 바꾸므로 이 경로가 정상 경로다.
   */
  if (addr->addr_type == BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE) {
    for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
      if (!slot_used(i)) continue;

      const bond_record_t *r = slot_at(i);
      if (r->role != role) continue;

      if (bondResolveAddress(addr, &r->keys.peer_id.id_info)) return (int8_t) i;
    }
  }
  return -1;
}

static int8_t slot_free(void)
{
  for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
    if (!slot_used(i)) return (int8_t) i;
  }
  return -1;
}

void bondInit(void)
{
  /* RRAM 은 준비 과정이 없다. 파티션 크기만 확인해 둔다. */
  (void) __bond_storage_size__;
}

bool bondSaveKeys(uint8_t role, const bond_keys_t *keys)
{
  if (keys == NULL) return false;

  const ble_gap_addr_t *id = &keys->peer_id.id_addr_info;

  int8_t i = slot_find(role, id);
  if (i < 0) i = slot_free();
  /*
   * 자리가 없으면 **덮어쓰지 않고 실패시킨다.** 어떤 것을 버릴지는 우리가
   * 정할 문제가 아니다. 스케치가 bondClear() 로 비우게 한다.
   */
  if (i < 0) return false;

  bond_record_t rec;
  memset(&rec, 0, sizeof(rec));
  rec.magic = BOND_MAGIC;
  rec.role  = role;
  rec.keys  = *keys;
  /* 시스템 속성은 아직 모른다. 나중에 bondSaveCccd() 가 채운다. */
  rec.sys_attr_len = 0;

  return slot_write((uint8_t) i, &rec);
}

bool bondLoadKeys(uint8_t role, const ble_gap_addr_t *peer_addr, bond_keys_t *keys)
{
  if (peer_addr == NULL || keys == NULL) return false;

  int8_t i = slot_find(role, peer_addr);
  if (i < 0) return false;

  *keys = slot_at((uint8_t) i)->keys;
  return true;
}

bool bondSaveCccd(uint8_t role, uint16_t conn_hdl, const ble_gap_addr_t *peer_addr)
{
  if (peer_addr == NULL) return false;

  int8_t i = slot_find(role, peer_addr);
  if (i < 0) return false;

  uint8_t  buf[BOND_SYS_ATTR_MAX];
  uint16_t len = sizeof(buf);

  /* CCCD 만 받는다. 사용자 정의 속성까지 가져오면 슬롯을 넘길 수 있다. */
  if (sd_ble_gatts_sys_attr_get(conn_hdl, buf, &len,
                                BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS) != NRF_SUCCESS) {
    return false;
  }
  if (len > BOND_SYS_ATTR_MAX) return false;

  bond_record_t rec = *slot_at((uint8_t) i);
  rec.sys_attr_len = (uint8_t) len;
  memset(rec.sys_attr, 0, sizeof(rec.sys_attr));
  memcpy(rec.sys_attr, buf, len);

  return slot_write((uint8_t) i, &rec);
}

bool bondLoadCccd(uint8_t role, uint16_t conn_hdl, const ble_gap_addr_t *peer_addr)
{
  if (peer_addr == NULL) return false;

  int8_t i = slot_find(role, peer_addr);
  if (i < 0) return false;

  const bond_record_t *r = slot_at((uint8_t) i);
  if (r->sys_attr_len == 0) return false;

  return sd_ble_gatts_sys_attr_set(conn_hdl, r->sys_attr, r->sys_attr_len,
                                   BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS) == NRF_SUCCESS;
}

void bondClear(uint8_t role)
{
  for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
    if (!slot_used(i)) continue;
    if (slot_at(i)->role != role) continue;

    /* magic 만 지우면 빈 슬롯이 된다 — 굳이 전체를 쓸 이유가 없다. */
    bond_record_t rec;
    memset(&rec, 0, sizeof(rec));
    slot_write(i, &rec);
  }
}

void bondClearAll(void)
{
  bond_record_t rec;
  memset(&rec, 0, sizeof(rec));

  for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
    if (slot_used(i)) slot_write(i, &rec);
  }
}

uint8_t bondCount(uint8_t role)
{
  uint8_t n = 0;
  for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
    if (!slot_used(i)) continue;
    if (role != BLE_GAP_ROLE_INVALID && slot_at(i)->role != role) continue;
    n++;
  }
  return n;
}

void bondPrintList(uint8_t role)
{
  Serial.printf("bond slots (%u/%u used)\n", bondCount(BLE_GAP_ROLE_INVALID), BOND_MAX_COUNT);

  for (uint8_t i = 0; i < BOND_MAX_COUNT; i++) {
    if (!slot_used(i)) continue;

    const bond_record_t *r = slot_at(i);
    if (role != BLE_GAP_ROLE_INVALID && r->role != role) continue;

    const uint8_t *a = r->keys.peer_id.id_addr_info.addr;
    Serial.printf("  [%2u] %s %02X:%02X:%02X:%02X:%02X:%02X  sys_attr %u B\n",
                  i, (r->role == BLE_GAP_ROLE_PERIPH) ? "prph" : "cntr",
                  a[5], a[4], a[3], a[2], a[1], a[0], r->sys_attr_len);
  }
}

bool bondResolveAddress(const ble_gap_addr_t *addr, const ble_gap_irk_t *irk)
{
  if (addr == NULL || irk == NULL) return false;
  if (addr->addr_type != BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE) return false;

  /*
   * resolvable private 주소는 prand(3바이트) + hash(3바이트) 다.
   * hash == ah(IRK, prand) 이면 그 IRK 의 주인이다.
   *
   * ⚠ 바이트 순서가 함정이다. addr 와 IRK 는 리틀엔디안인데 AES 블록은
   *   빅엔디안이라 키·입력·출력을 전부 뒤집어야 한다. 안 뒤집으면 늘 불일치가
   *   나고, 증상은 "본딩했는데 재연결 때 못 알아본다" 로만 보인다.
   */
  const uint8_t *hash = &addr->addr[0];
  const uint8_t *rand = &addr->addr[3];

  nrf_ecb_hal_data_t ecb;
  memset(&ecb, 0, sizeof(ecb));

  for (uint8_t i = 0; i < SOC_ECB_KEY_LENGTH; i++) {
    ecb.key[i] = irk->irk[SOC_ECB_KEY_LENGTH - 1 - i];
  }
  /* prand 를 블록 끝에 빅엔디안으로 둔다 (앞은 0 패딩). */
  ecb.cleartext[SOC_ECB_CLEARTEXT_LENGTH - 3] = rand[2];
  ecb.cleartext[SOC_ECB_CLEARTEXT_LENGTH - 2] = rand[1];
  ecb.cleartext[SOC_ECB_CLEARTEXT_LENGTH - 1] = rand[0];

  if (sd_ecb_block_encrypt(&ecb) != NRF_SUCCESS) return false;

  /* 결과의 하위 3바이트가 hash 다. 빅엔디안이므로 뒤에서 가져온다. */
  return (ecb.ciphertext[SOC_ECB_CIPHERTEXT_LENGTH - 1] == hash[0]) &&
         (ecb.ciphertext[SOC_ECB_CIPHERTEXT_LENGTH - 2] == hash[1]) &&
         (ecb.ciphertext[SOC_ECB_CIPHERTEXT_LENGTH - 3] == hash[2]);
}
