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

/**
 * 옛 GATT 배치의 CCCD 를 버린 횟수 (진단용).
 *
 * `Serial` 이 죽은 상태에서도 SWD 로 읽을 수 있게 전역으로 둔다 —
 * 이 프로젝트의 다른 진단들과 같은 방식이다 (CLAUDE.md §8.3).
 */
volatile uint32_t g_bond_cccd_stale = 0;

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
  /**
   * sys_attr 을 저장했을 때의 GATT 지문 (AdafruitBluefruit::_gattFingerprint()).
   *
   * ⚠ 이것이 없으면 다음 함정에 빠진다.
   *   sys_attr 은 **속성 핸들 기준**이라 스케치를 바꿔 GATT 구성이 달라지면
   *   무의미해진다. 그런데 호스트는 본딩이 살아 있으니 CCCD 를 다시 쓰지 않고,
   *   결과가 "연결·암호화는 되는데 알림만 안 온다" 로 나타난다.
   *   폴트도 로그도 없어 원인을 찾기가 매우 어렵다.
   *
   * ⚠ **반드시 구조체 끝에 둔다.** 앞이나 중간에 넣으면 뒤 필드가 4바이트씩
   *   밀려 이 코어를 올리기 전에 저장된 레코드의 키가 어긋난다 (매직을 올려
   *   통째로 버려야 했을 것이다). 끝에 두면 옛 레코드는 슬롯의 남는 자리를
   *   읽는데, slot_write() 가 슬롯 전체를 0 으로 채우고 쓰므로 **0 이 나온다.**
   *   0 은 어떤 지문과도 안 맞으니 "CCCD 만 버리고 키는 살린다" 가 된다.
   *   그래서 코어를 올려도 다시 페어링할 필요가 없다.
   */
  uint32_t       gatt_fp;
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
  rec.gatt_fp      = 0;

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

  /*
   * ⚠ **시스템 서비스와 사용자 서비스를 모두 가져와야 한다.**
   *   SYS_SRVCS 만 주면 Service Changed 같은 시스템 CCCD 만 담기고,
   *   NUS 처럼 **우리가 만든 서비스의 CCCD 는 빠진다.** 그러면 저장은 되는데
   *   (8 바이트쯤) 재연결 때 알림이 여전히 꺼져 있다. 실제로 그 증상을 겪었다.
   */
  if (sd_ble_gatts_sys_attr_get(conn_hdl, buf, &len,
                                BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS |
                                BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS) != NRF_SUCCESS) {
    return false;
  }
  if (len > BOND_SYS_ATTR_MAX) return false;

  bond_record_t rec = *slot_at((uint8_t) i);
  rec.sys_attr_len = (uint8_t) len;
  rec.gatt_fp      = Bluefruit._gattFingerprint();
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

  /*
   * GATT 구성이 그때와 다르면 저장된 CCCD 는 **다른 핸들을 가리킨다.**
   * 복원하지 않고 버린다 — 잘못 복원하면 엉뚱한 characteristic 의 알림이
   * 켜지거나, 켜졌다고 착각한 채 아무것도 안 온다.
   *
   * ⚠ 버리는 것만으로는 호스트가 다시 구독하지 않는다. 호스트는 본딩이
   *   살아 있으니 CCCD 를 이미 썼다고 믿는다. 그래서 **경고를 남긴다** —
   *   이게 없으면 "알림이 안 온다" 는 증상만 남고 원인이 안 보인다.
   *   자동 복구는 Service Changed 를 켜야 되는데 그건 SoftDevice 구성
   *   변경이라 별도 작업이다 (docs/STATUS.md).
   */
  if (r->gatt_fp != Bluefruit._gattFingerprint()) {
    g_bond_cccd_stale++;
    /* printf 를 쓰지 않는다 — 이 파일은 BLE 스케치 전부에 링크되는데
     * printf 하나가 15 KB 를 끌고 온다 (실측). */
    Serial.print("bond: stored CCCD is for a different GATT layout (");
    Serial.print(r->gatt_fp, HEX);
    Serial.print(" != ");
    Serial.print(Bluefruit._gattFingerprint(), HEX);
    Serial.println("). Dropped - re-pair to get notifications back.");
    return false;
  }

  /* 저장할 때와 **같은 플래그**여야 한다 (위 주석 참조). */
  return sd_ble_gatts_sys_attr_set(conn_hdl, r->sys_attr, r->sys_attr_len,
                                   BLE_GATTS_SYS_ATTR_FLAG_SYS_SRVCS |
                                   BLE_GATTS_SYS_ATTR_FLAG_USR_SRVCS) == NRF_SUCCESS;
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
    Serial.printf("  [%2u] %s %02X:%02X:%02X:%02X:%02X:%02X  sys_attr %u B  gatt %08lX\n",
                  i, (r->role == BLE_GAP_ROLE_PERIPH) ? "prph" : "cntr",
                  a[5], a[4], a[3], a[2], a[1], a[0], r->sys_attr_len,
                  (unsigned long) r->gatt_fp);
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
